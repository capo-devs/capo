#pragma once
#include <impl_al.hpp>
#include <ktl/kthread.hpp>

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
	template <std::size_t N>
	struct Primer {
		StreamFrame<N> frames[BufferCount]{};
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
	template <std::size_t N>
	bool enqueue(Primer<N> const& primer) {
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
template <std::size_t BufferSize = 3, std::size_t FrameSize = 4096>
class StreamSource {
  public:
	using Primer = typename StreamBuffer<BufferSize>::template Primer<FrameSize>;

	StreamSource() : m_buffer(m_source.value) {}

	ALuint source() const noexcept { return m_source.value; }
	void loop(bool value) noexcept { m_loop = value; }
	bool looping() const noexcept { return m_loop; }

	Outcome open(std::string path) {
		if (auto res = m_streamer.open(std::move(path))) { return start(); }
		return Error::eIOError;
	}

	Outcome load(PCM pcm) {
		m_streamer.preload(std::move(pcm));
		return start();
	}

	Outcome reset() {
		if (ready()) { return m_streamer.reopen(); }
		return Error::eInvalidValue;
	}

	bool valid() const noexcept { return m_streamer.valid(); }
	bool ready() const { return valid() && m_buffer.queued(); }
	PCM::Streamer const& streamer() const noexcept { return m_streamer; }

  private:
	struct Source {
		ALuint value;

		Source() : value(genSource()) {}
		~Source() {
			ALuint const src[] = {value};
			deleteSources(src);
		}
	};

	bool tick() {
		if (m_streamer.remain() == 0) {
			if (m_loop) {
				return m_streamer.reopen().has_value(); // attempt reopen
			} else {
				return false; // playback complete
			}
		}
		return true; // keep ticking
	}

	Outcome start() {
		Primer primer(m_streamer);
		if (m_buffer.enqueue(primer)) {
			m_thread = ktl::kthread([this](ktl::kthread::stop_t stop) {
				StreamFrame<FrameSize> next;
				SamplesView samples = SamplesView(next, m_streamer.read(next));
				while (!stop.stop_requested()) {
					if (m_buffer.next(samples)) {
						samples = SamplesView(next, m_streamer.read(next));
					} else {
						ktl::kthread::yield();
					}
					if (!tick()) { break; }
				}
			});
			m_thread.m_join = ktl::kthread::policy::stop;
			return Outcome::success();
		}
		return Error::eInvalidData;
	}

	Source m_source;
	StreamBuffer<BufferSize> m_buffer;
	PCM::Streamer m_streamer;
	ktl::kthread m_thread;
	bool m_loop{};
};
} // namespace capo::detail
