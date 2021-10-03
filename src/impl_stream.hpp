#pragma once
#include <impl_al.hpp>
#include <ktl/kthread.hpp>
#include <ktl/tmutex.hpp>
#include <atomic>

namespace capo::detail {
///
/// \brief One streaming unit
///
template <std::size_t N>
using StreamFrame = PCM::Sample[N];

///
/// \brief Ringbuffer of OpenAL buffers
///
template <std::size_t BufferCount>
class StreamBuffer {
  public:
	///
	/// \brief Helper to prime buffers with init data
	///
	template <std::size_t FrameSize>
	struct Primer {
		StreamFrame<FrameSize> frames[BufferCount]{};
		Metadata meta;

		Primer(PCM::Streamer& out) noexcept : meta(out.meta()) {
			for (auto& frame : frames) { out.read(frame); }
		}
	};

	StreamBuffer(ALuint source) : m_source(source) {
		setSourceProp(m_source, AL_BUFFER, 0);
		for (auto& buf : m_buffers) { buf = genBuffer(); }
	}

	~StreamBuffer() {
		CAPO_CHK(alSourceStop(m_source));	   // stop playing
		drainQueue(m_source);				   // unqueue all buffers
		setSourceProp(m_source, AL_BUFFER, 0); // unbind buffers
		deleteBuffers(m_buffers);
	}

	// prime buffers and enqueue them
	template <std::size_t FrameSize>
	bool enqueue(Primer<FrameSize> const& primer) {
		m_meta = primer.meta;
		std::size_t i{};
		for (SamplesView const frame : primer.frames) { bufferData(m_buffers[i++], m_meta, frame); } // prime buffers
		CAPO_CHKR(alSourceQueueBuffers(m_source, static_cast<ALsizei>(BufferCount), m_buffers));	 // queue buffers
		return true;
	}

	bool queued() const { return detail::getSourceProp<ALint>(m_source, AL_BUFFERS_QUEUED) > 0; }

	// fill next buffer if vacant
	bool next(SamplesView samples) {
		if (canPopBuffer(m_source)) {		  // check if any buffers are vacant
			auto buf = popBuffer(m_source);	  // pop vacant buffer
			bufferData(buf, m_meta, samples); // write next frame
			ALuint const bufs[] = {buf};	  // prep buffer
			pushBuffers(m_source, bufs);	  // enqueue buffer
			return true;
		}
		return false;
	}

  private:
	ALuint m_buffers[BufferCount] = {};
	Metadata m_meta;
	ALuint m_source;
};

///
/// \brief OpenAL source with streaming / queued buffers
///
template <std::size_t BufferCount = 3, std::size_t FrameSize = 4096>
class StreamSource {
  public:
	using Primer = typename StreamBuffer<BufferCount>::template Primer<FrameSize>;

	StreamSource() : m_buffer(m_source.value) { start(); }

	ALuint source() const noexcept { return m_source.value; }
	void loop(bool value) noexcept { m_loop.store(value); }
	bool looping() const noexcept { return m_loop.load(); }

	Result<void> open(std::string path) {
		ktl::tlock lock(m_streamer);
		if (lock->open(std::move(path)) && m_buffer.enqueue(Primer(*lock))) { return Result<void>::success(); }
		return Error::eIOError;
	}

	Result<void> load(PCM pcm) {
		ktl::tlock lock(m_streamer);
		lock->preload(std::move(pcm));
		if (m_buffer.enqueue(Primer(*lock))) { return Result<void>::success(); }
		return Error::eInvalidData;
	}

	Result<void> reset() {
		if (ready()) { return ktl::tlock(m_streamer)->reopen(); }
		return Error::eInvalidValue;
	}

	Result<void> seek(Time stamp) {
		if (ready()) { return ktl::tlock(m_streamer)->seek(stamp); }
		return Error::eInvalidValue;
	}

	Time elapsed() const { return ktl::tlock(m_streamer)->position(); }

	bool valid() const { return ktl::tlock(m_streamer)->valid(); }
	bool ready() const { return valid() && m_buffer.queued(); }
	// immutable ref doesn't have to be used under a lock, unlocking and returning is ok
	PCM::Streamer const& streamer() const { return *ktl::tlock(m_streamer); }

  private:
	struct Source {
		ALuint value;

		Source() : value(genSource()) {}
		~Source() {
			ALuint const src[] = {value};
			deleteSources(src);
		}
	};

	void start() {
		m_thread = ktl::kthread([this](ktl::kthread::stop_t stop) {
			StreamFrame<FrameSize> next;
			SamplesView samples = SamplesView(next, ktl::tlock(m_streamer)->read(next));
			while (!stop.stop_requested()) {
				if (m_buffer.next(samples)) { samples = SamplesView(next, ktl::tlock(m_streamer)->read(next)); }
				tick();
				ktl::kthread::yield();
			}
		});
		m_thread.m_join = ktl::kthread::policy::stop;
	}

	void tick() {
		ktl::tlock lock(m_streamer);
		if (lock->remain() == 0) {
			// playback complete, stop source if not looping
			if (!m_loop.load()) { CAPO_CHK(alSourceStop(m_source.value)); }
			lock->reopen(); // rewind to start
		}
	}

	Source m_source;							  // read-only after construction
	StreamBuffer<BufferCount> m_buffer;			  // only accessed in worker thread (after priming)
	ktl::strict_tmutex<PCM::Streamer> m_streamer; // accessed across both threads
	ktl::kthread m_thread;						  // thread
	std::atomic_bool m_loop;					  // accessed across both threads
};
} // namespace capo::detail
