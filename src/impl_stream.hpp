#pragma once
#include <impl_al.hpp>
#include <ktl/async/kthread.hpp>
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
		set_source_prop(m_source, AL_BUFFER, 0); // unbind any existing buffers
		for (auto& buf : m_buffers) { buf = gen_buffer(); }
	}

	~StreamBuffer() {
		stop_source(m_source);					 // stop playing
		release();								 // unqueue all buffers
		set_source_prop(m_source, AL_BUFFER, 0); // unbind buffers
		delete_buffers(m_buffers);
	}

	// prime all buffers and enqueue them
	template <std::size_t FrameSize>
	bool acquire(Primer<FrameSize> const& primer, Metadata const& meta) {
		m_meta = meta;
		std::size_t i{};
		for (SamplesView const frame : primer) { buffer_data(m_buffers[i++], m_meta, frame); }
		return push_buffers(m_source, m_buffers);
	}

	// dequeue all vacant buffers
	std::size_t release() {
		std::size_t ret{};
		// unqueue all buffers
		while (can_pop_buffer(m_source)) {
			pop_buffer(m_source);
			++ret;
		}
		return ret;
	}

	// number of buffers enqueued (0 when released / not primed, BufferCount when acquired / primed)
	std::size_t queued() const { return static_cast<std::size_t>(get_source_prop<ALint>(m_source, AL_BUFFERS_QUEUED)); }
	// number of enqueued buffers ready to be popped
	std::size_t vacant() const { return static_cast<std::size_t>(get_source_prop<ALint>(m_source, AL_BUFFERS_PROCESSED)); }

	// fill and enqueue next buffer if vacant
	bool next(SamplesView samples) {
		if (can_pop_buffer(m_source)) {		   // check if any buffers are vacant
			auto buf = pop_buffer(m_source);   // pop vacant buffer
			buffer_data(buf, m_meta, samples); // write next frame
			ALuint const bufs[] = {buf};	   // prep buffer
			push_buffers(m_source, bufs);	   // enqueue buffer
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

	bool open(char const* path) {
		std::scoped_lock lock(m_mutex);
		return m_streamer.open(path).has_value();
	}

	void load(PCM pcm) {
		std::scoped_lock lock(m_mutex);
		m_streamer.preload(std::move(pcm));
	}

	bool play() {
		std::scoped_lock lock(m_mutex);
		return play(lock);
	}

	bool stop() {
		std::scoped_lock lock(m_mutex);
		return stop(lock);
	}

	bool playing() const { return get_source_prop<ALint>(m_source.value, AL_SOURCE_STATE) == AL_PLAYING; }
	bool paused() const { return get_source_prop<ALint>(m_source.value, AL_SOURCE_STATE) == AL_PAUSED; }

	bool rewind() {
		std::scoped_lock lock(m_mutex);
		// rewind stream
		if (m_streamer.valid() && m_streamer.seek({}).has_value()) {
			// rewind source
			rewind_source(m_source.value);
			return true;
		}
		return false;
	}

	bool seek(Time stamp) {
		std::scoped_lock lock(m_mutex);
		bool const resume = playing();
		// stop even if paused (drain queue)
		stop(lock);
		bool const ret = m_streamer.valid() && m_streamer.seek(stamp).has_value();
		if (resume) { play(lock); }
		return ret;
	}

	Time position() const {
		std::scoped_lock lock(m_mutex);
		if (!m_streamer.valid()) { return Time(); }
		// compute how many samples ahead of playback streamer.remain() is
		auto const samplesAhead = (m_buffer.queued() - m_buffer.vacant() + (playing() || paused() ? 1 : 0)) * FrameSize;
		// find corresponding progress along track
		auto const progress = detail::stream_progress(m_streamer.sample_count(), m_streamer.remain() + samplesAhead);
		// find source's position on current buffer
		auto const offset = Time(get_source_prop<float>(m_source.value, AL_SEC_OFFSET));
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

		Source() : value(gen_source()) {}
		~Source() {
			ALuint const src[] = {value};
			delete_sources(src);
		}
	};
	using Lock = std::scoped_lock<std::mutex>;

	// need to acquire before playback if true
	bool empty(Lock const&) const { return m_buffer.queued() == 0; }
	// need to release and re-acquire before playback if true
	bool starved(Lock const&) const { return m_buffer.vacant() == BufferCount; }

	void release(Lock const&) {
		[[maybe_unused]] std::size_t const d = m_buffer.release();
		assert(m_buffer.queued() == 0);
	}

	bool acquire(Lock const&) {
		Primer primer = {};
		for (auto& frame : primer) { m_streamer.read(frame); }
		return m_buffer.acquire(primer, m_streamer.meta());
	}

	bool play(Lock const& lock) {
		if (m_streamer.valid()) {
			// rewind
			if (m_streamer.remain() == 0 && !m_streamer.seek({})) { return false; }
			// drain queue if starved (stopped by itself)
			if (starved(lock)) { release(lock); }
			// prime queue if cold start (not unpause)
			if (empty(lock)) {
				// prime buffers
				if (!acquire(lock)) { return false; }
				// prepare next frame for poll thread (which will copy it into next available buffer)
				m_next = SamplesView(m_frame_storage, m_streamer.read(m_frame_storage));
			}
			return play_source(m_source.value);
		}
		return false;
	}

	bool stop(Lock const& lock) {
		if (m_streamer.valid() && stop_source(m_source.value)) {
			// drain queue
			release(lock);
			m_streamer.seek({});
			// return true even if seek fails; stop succeeded
			return true;
		}
		return false;
	}

	void tick() {
		std::scoped_lock lock(m_mutex);
		// refresh next frame if queued into buffer
		if (m_buffer.next(m_next)) { m_next = SamplesView(m_frame_storage, m_streamer.read(m_frame_storage)); }
		// rewind if looping and stream has finished
		if (m_loop.load() && m_streamer.remain() == 0) { m_streamer.seek({}); } // rewind
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
	StreamFrame<FrameSize> m_frame_storage;

	// "dependent" types, order matters
	Source m_source;
	StreamBuffer<BufferCount> m_buffer;

	// regular members
	PCM::Streamer m_streamer;
	SamplesView m_next;
	mutable std::mutex m_mutex;
	std::atomic_bool m_loop;

	// must be destroyed first
	ktl::kthread m_thread;
};
} // namespace capo::detail
