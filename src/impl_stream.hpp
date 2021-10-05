#pragma once
#include <impl_al.hpp>
#include <ktl/kthread.hpp>
#include <atomic>
#include <mutex>

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
	template <std::size_t FrameSize>
	using Primer = StreamFrame<FrameSize>[BufferCount];

	StreamBuffer(ALuint source) : m_source(source) {
		setSourceProp(m_source, AL_BUFFER, 0); // unbind any existing buffers
		for (auto& buf : m_buffers) { buf = genBuffer(); }
	}

	~StreamBuffer() {
		stopSource(m_source);				   // stop playing
		release();							   // unqueue all buffers
		setSourceProp(m_source, AL_BUFFER, 0); // unbind buffers
		deleteBuffers(m_buffers);
	}

	// prime all buffers and enqueue them
	template <std::size_t FrameSize>
	bool acquire(Primer<FrameSize> const& primer, Metadata const& meta) {
		m_meta = meta;
		std::size_t i{};
		for (SamplesView const frame : primer) { bufferData(m_buffers[i++], m_meta, frame); } // prime buffers
		return pushBuffers(m_source, m_buffers);
	}

	// dequeue all vacant buffers
	std::size_t release() {
		std::size_t ret{};
		// unqueue all buffers
		while (canPopBuffer(m_source)) {
			popBuffer(m_source);
			++ret;
		}
		return ret;
	}

	// number of buffers enqueued (0 when released / not primed, BufferCount when acquired / primed)
	std::size_t queued() const { return static_cast<std::size_t>(getSourceProp<ALint>(m_source, AL_BUFFERS_QUEUED)); }
	// number of enqueued buffers ready to be popped
	std::size_t vacant() const { return static_cast<std::size_t>(getSourceProp<ALint>(m_source, AL_BUFFERS_PROCESSED)); }

	// fill and enqueue next buffer if vacant
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

	bool open(std::string path) {
		std::scoped_lock lock(m_mutex);
		return m_streamer.open(std::move(path)).has_value();
	}

	void load(PCM pcm) {
		std::scoped_lock lock(m_mutex);
		m_streamer.preload(std::move(pcm));
	}

	bool play() {
		std::scoped_lock lock(m_mutex);
		return playImpl();
	}

	bool stop() {
		std::scoped_lock lock(m_mutex);
		return stopImpl();
	}

	bool playing() const { return getSourceProp<ALint>(m_source.value, AL_SOURCE_STATE) == AL_PLAYING; }
	bool paused() const { return getSourceProp<ALint>(m_source.value, AL_SOURCE_STATE) == AL_PAUSED; }

	bool rewind() {
		std::scoped_lock lock(m_mutex);
		// rewind stream
		if (m_streamer.valid() && m_streamer.reopen().has_value()) {
			// rewind source
			rewindSource(m_source.value);
			return true;
		}
		return false;
	}

	bool seek(Time stamp) {
		std::scoped_lock lock(m_mutex);
		bool const resume = playing();											   // resume playback after seek
		stopImpl();																   // stop even if paused (drain queue)
		bool const ret = m_streamer.valid() && m_streamer.seek(stamp).has_value(); // seek
		if (resume) { playImpl(); }												   // resume playback
		return ret;
	}

	Time position() const {
		std::scoped_lock lock(m_mutex);
		// compute how many samples ahead of playback streamer.remain() is
		auto const samplesAhead = (m_buffer.queued() - m_buffer.vacant() + (playing() || paused() ? 1 : 0)) * FrameSize;
		// find corresponding progress along track
		auto const progress = m_streamer.progress(m_streamer.remain() + samplesAhead);
		// find source's position on current buffer
		auto const offset = Time(getSourceProp<float>(m_source.value, AL_SEC_OFFSET));
		return progress * m_streamer.meta().length() + offset;
	}

	bool ready() const {
		std::scoped_lock lock(m_mutex);
		return m_streamer.valid();
	}

	// immutable ref doesn't have to be used under> a lock, unlocking and returning is ok
	PCM::Streamer const& streamer() const {
		std::scoped_lock lock(m_mutex);
		return m_streamer;
	}

  private:
	struct Source {
		ALuint value;

		Source() : value(genSource()) {}
		~Source() {
			ALuint const src[] = {value};
			deleteSources(src);
		}
	};

	// need to acquire before playback if true
	bool queueEmpty() const { return m_buffer.queued() == 0; }
	// need to release and re-acquire before playback if true
	bool queueStarved() const { return m_buffer.vacant() == BufferCount; }

	void release() {
		[[maybe_unused]] std::size_t const d = m_buffer.release();
		assert(m_buffer.queued() == 0);
	}

	bool acquire() {
		Primer primer = {};
		for (auto& frame : primer) { m_streamer.read(frame); }
		return m_buffer.acquire(primer, m_streamer.meta());
	}

	bool playImpl() {
		if (m_streamer.valid()) {
			// rewind
			if (m_streamer.remain() == 0 && !m_streamer.reopen()) { return false; }
			// drain queue if starved (stopped by itself)
			if (queueStarved()) { release(); }
			// prime queue if cold start (not unpause)
			if (queueEmpty()) {
				// prime buffers
				if (!acquire()) { return false; }
				// prepare next frame for poll thread (which will copy it into next available buffer)
				m_next = SamplesView(m_frameStorage, m_streamer.read(m_frameStorage));
			}
			return playSource(m_source.value);
		}
		return false;
	}

	bool stopImpl() {
		if (m_streamer.valid() && stopSource(m_source.value)) {
			release();			 // drain queue
			m_streamer.reopen(); // rewind
			return true;		 // return true even if rewind fails; stop succeeded
		}
		return false;
	}

	void tick() {
		std::scoped_lock lock(m_mutex);
		// refresh next frame if queued into buffer
		if (m_buffer.next(m_next)) { m_next = SamplesView(m_frameStorage, m_streamer.read(m_frameStorage)); }
		// reopen if looping and stream has finished
		if (m_loop.load() && m_streamer.remain() == 0) { m_streamer.reopen(); } // rewind
	}

	void start() {
		m_thread = ktl::kthread([this](ktl::kthread::stop_t stop) {
			while (!stop.stop_requested()) {
				tick();				   // lock mutex in here...
				ktl::kthread::yield(); // then yield outside lock
			}
		});
		m_thread.m_join = ktl::kthread::policy::stop;
	}

	// huge buffer on top
	StreamFrame<FrameSize> m_frameStorage;

	// "dependent" types, order matters
	Source m_source;
	StreamBuffer<BufferCount> m_buffer;
	PCM::Streamer m_streamer;

	// regular members
	SamplesView m_next;
	ktl::kthread m_thread;
	mutable std::mutex m_mutex;
	std::atomic_bool m_loop;
};
} // namespace capo::detail
