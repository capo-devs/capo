#include <capo/instance.hpp>
#include <capo/pcm.hpp>
#include <capo/sound.hpp>
#include <capo/source.hpp>
#include <impl_al.hpp>
#include <ktl/kthread.hpp>
#include <array>
#include <cassert>

namespace capo {
namespace {
#define MU [[maybe_unused]]
using SamplesView = std::span<PCM::Sample const>;

constexpr ALenum g_alFormats[] = {AL_FORMAT_MONO16, AL_FORMAT_STEREO16};
constexpr ALenum alFormat(capo::SampleFormat format) noexcept { return g_alFormats[static_cast<std::size_t>(format)]; }

ALuint genBuffer() noexcept(false) {
	ALuint ret{};
	CAPO_CHKR(alGenBuffers(1, &ret));
	return ret;
}

ALuint genSource() noexcept(false) {
	ALuint ret{};
	CAPO_CHKR(alGenSources(1, &ret));
	return ret;
}

void deleteBuffers(MU std::span<ALuint const> buffers) noexcept(false) { CAPO_CHK(alDeleteBuffers(static_cast<ALsizei>(buffers.size()), buffers.data())); }

void deleteSources(MU std::span<ALuint const> sources) noexcept(false) { CAPO_CHK(alDeleteSources(static_cast<ALsizei>(sources.size()), sources.data())); }

template <typename Cont>
void bufferData(MU ALuint buffer, MU ALenum format, MU Cont const& data, MU std::size_t freq) noexcept(false) {
	CAPO_CHK(alBufferData(buffer, format, data.data(), static_cast<ALsizei>(data.size()) * sizeof(typename Cont::value_type), static_cast<ALsizei>(freq)));
}

void bufferData(MU ALuint buffer, MU SampleMeta const& meta, MU SamplesView samples) noexcept(false) {
	bufferData(buffer, alFormat(meta.format), samples, meta.rate);
}

ALuint genBuffer(MU SampleMeta const& meta, MU SamplesView samples) noexcept(false) {
	auto ret = genBuffer();
	bufferData(ret, meta, samples);
	return ret;
}

bool canPopBuffer(MU ALuint source) noexcept(false) {
	ALint vacant{};
	CAPO_CHKR(alGetSourcei(source, AL_BUFFERS_PROCESSED, &vacant));
	return vacant > 0;
}

ALuint popBuffer(MU ALuint source) noexcept(false) {
	assert(canPopBuffer(source));
	ALuint ret{};
	CAPO_CHKR(alSourceUnqueueBuffers(source, 1, &ret));
	return ret;
}

bool pushBuffers(MU ALuint source, MU std::span<ALuint const> buffers) noexcept(false) {
	CAPO_CHKR(alSourceQueueBuffers(source, static_cast<ALsizei>(buffers.size()), buffers.data()));
	return true;
}

void drainQueue(ALuint source) noexcept(false) {
	while (canPopBuffer(source)) { popBuffer(source); }
}

template <std::size_t N>
using StreamFrame = PCM::Sample[N];

template <std::size_t BufferCount>
class StreamBuffer {
  public:
	template <std::size_t N>
	struct Primer {
		StreamFrame<N> frames[BufferCount]{};
		SampleMeta meta;

		Primer(PCM::Streamer& out) noexcept : meta(out.meta()) {
			for (auto& frame : frames) { out.read(frame); }
		}
	};

	StreamBuffer(UID source) : m_source(source) {
		detail::setSourceProp(m_source, AL_BUFFER, 0);
		for (auto& buf : m_buffers) { buf = genBuffer(); }
	}

	~StreamBuffer() {
		CAPO_CHK(alSourceStop(m_source));			   // stop playing
		drainQueue(m_source);						   // unqueue all buffers
		detail::setSourceProp(m_source, AL_BUFFER, 0); // unbind buffers
		deleteBuffers(m_buffers);
	}

	template <std::size_t N>
	bool enqueue(Primer<N> const& primer) {
		m_meta = primer.meta;
		std::size_t i{};
		for (SamplesView const frame : primer.frames) { bufferData(m_buffers[i++], m_meta, frame); }
		CAPO_CHKR(alSourceQueueBuffers(m_source, static_cast<ALsizei>(BufferCount), m_buffers));
		return true;
	}

	bool next(SamplesView samples) {
		if (canPopBuffer(m_source)) {
			auto buf = popBuffer(m_source);
			bufferData(buf, m_meta, samples);
			ALuint const bufs[] = {buf};
			pushBuffers(m_source, bufs);
			return true;
		}
		return false;
	}

  private:
	ALuint m_buffers[BufferCount] = {};
	SampleMeta m_meta;
	UID m_source;
};

template <std::size_t BufferSize = 3, std::size_t FrameSize = 4096>
class StreamSource {
  public:
	using Primer = typename StreamBuffer<BufferSize>::template Primer<FrameSize>;

	StreamSource() : m_buffer(m_source.value) {}

	Outcome open(std::string path) {
		if (auto res = m_streamer.open(std::move(path))) { return start(); }
		return Error::eIOError;
	}

	Outcome load(PCM pcm) {
		m_streamer.preload(std::move(pcm));
		return start();
	}

	Outcome TEST_play(float gain) {
		if (m_streamer.valid()) {
			detail::setSourceProp(m_source.value, AL_GAIN, gain);
			CAPO_CHKR(alSourcePlay(m_source.value));
			if (detail::getSourceProp<ALint>(m_source.value, AL_SOURCE_STATE) != AL_PLAYING) { return Error::eUnknown; }
			while (detail::getSourceProp<ALint>(m_source.value, AL_SOURCE_STATE) == AL_PLAYING) { std::this_thread::yield(); }
			return Outcome::success();
		}
		return Error::eInvalidData;
	}

  private:
	struct Source {
		UID value;

		Source() : value(genSource()) {}
		~Source() {
			ALuint const src[] = {value};
			deleteSources(src);
		}
	};

	bool tick() {
		if (m_streamer.remain() == 0) {
			if (detail::getSourceProp<ALint>(m_source.value, AL_LOOPING) == AL_TRUE) {
				return m_streamer.reopen().has_value();
			} else {
				return false;
			}
		}
		return true;
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
						std::this_thread::yield();
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
};

#undef MU
} // namespace

Outcome TEST_stream(std::string_view path, float gain) {
	StreamSource ss;
	if (auto pcm = PCM::fromFile(std::string(path))) {
		if (auto outcome = ss.load(std::move(*pcm))) {
			return ss.TEST_play(gain);
		} else {
			return outcome.error();
		}
	}
	if (auto outcome = ss.open(std::string(path))) {
		return ss.TEST_play(gain);
	} else {
		return outcome.error();
	}
	return Error::eUnknown;
}

Instance::Instance() {
#if defined(CAPO_USE_OPENAL)
	if (alcGetCurrentContext() != nullptr) {
		detail::onError(Error::eDuplicateInstance);
	} else {
		if (ALCdevice* device = alcOpenDevice(nullptr)) {
			if (ALCcontext* context = alcCreateContext(device, nullptr)) {
				m_device = device;
				m_context = context;
				CAPO_CHK(alcMakeContextCurrent(context));
			} else {
				detail::onError(Error::eContextFailure);
			}
		} else {
			detail::onError(Error::eDeviceFailure);
		}
	}
#endif
}

Instance::~Instance() {
#if defined(CAPO_USE_OPENAL)
	if (valid()) {
		std::vector<ALuint> resources;
		auto fill = [&resources](auto const& map) {
			resources.clear();
			resources.reserve(map.size());
			for (auto const& [id, _] : map) { resources.push_back(id); }
		};
		// delete all sources, implicitly unbinding all buffers
		fill(m_sources);
		deleteSources(resources);
		// delete all buffers
		fill(m_sounds);
		deleteBuffers(resources);
		// destroy context and close device
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(m_context.get<ALCcontext*>());
		alcCloseDevice(m_device.get<ALCdevice*>());
	}
#endif
}

bool Instance::valid() const noexcept { return use_openal_v ? m_device.contains<ALCdevice*>() && m_context.contains<ALCcontext*>() : valid_if_inactive_v; }

Sound const& Instance::makeSound(PCM const& pcm) {
	if (valid()) {
		auto buffer = genBuffer(pcm.meta, pcm.samples);
		auto [it, _] = m_sounds.insert_or_assign(buffer, Sound(this, buffer, Time((float)pcm.samples.size() / ((float)pcm.meta.rate * pcm.meta.channels))));
		return it->second;
	}
	return Sound::blank;
}

Source const& Instance::makeSource() {
	if (valid()) {
		auto source = genSource();
		auto [it, _] = m_sources.insert_or_assign(source, Source(this, source));
		return it->second;
	}
	return Source::blank;
}

bool Instance::destroy(Sound const& sound) {
	if (valid() && sound.valid()) {
		// unbind all sources
		for (UID const src : m_bindings.map[sound.m_buffer]) { detail::setSourceProp(src, AL_BUFFER, 0); }
		// delete buffer
		ALuint const buf[] = {sound.m_buffer};
		deleteBuffers(buf);
		// unmap buffer
		m_bindings.map.erase(sound.m_buffer);
		m_sounds.erase(sound.m_buffer);
		return true;
	}
	return false;
}

bool Instance::destroy(Source const& source) {
	if (valid() && source.valid()) {
		// delete source (implicitly unbinds buffer)
		ALuint const src[] = {source.m_handle};
		deleteSources(src);
		// unmap source
		m_bindings.unbind(source);
		m_sources.erase(source.m_handle);
		return true;
	}
	return false;
}

Sound const& Instance::findSound(UID id) const noexcept {
	if (auto it = m_sounds.find(id); it != m_sounds.end()) { return it->second; }
	return Sound::blank;
}

Source const& Instance::findSource(UID id) const noexcept {
	if (auto it = m_sources.find(id); it != m_sources.end()) { return it->second; }
	return Source::blank;
}

bool Instance::bind(Sound const& sound, Source const& source) {
	if (valid() && source.valid() && sound.valid()) {
		if (source.playing()) { CAPO_CHKR(alSourceStop(source.m_handle)); }
		if (detail::setSourceProp(source.m_handle, AL_BUFFER, static_cast<ALint>(sound.m_buffer))) {
			m_bindings.bind(sound, source);
			return true;
		}
	}
	return false;
}

bool Instance::unbind(Source const& source) {
	if (valid() && source.valid()) {
		if (source.playing()) { CAPO_CHKR(alSourceStop(source.m_handle)); }
		if (detail::setSourceProp(source.m_handle, AL_BUFFER, 0)) {
			m_bindings.unbind(source);
			return true;
		}
	}
	return false;
}

Sound const& Instance::bound(Source const& source) const noexcept {
	if (valid()) {
		for (auto const& [buf, set] : m_bindings.map) {
			if (set.contains(source.m_handle)) {
				auto it = m_sounds.find(buf);
				return it == m_sounds.end() ? Sound::blank : it->second;
			}
		}
	}
	return Sound::blank;
}

void Instance::Bindings::bind(Sound const& sound, Source const& source) {
	unbind(source);
	map[sound.m_buffer].insert(source.m_handle);
}

void Instance::Bindings::unbind(Source const& source) {
	for (auto& [_, set] : map) { set.erase(source.m_handle); }
}
} // namespace capo
