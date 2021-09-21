#include <al.h>
#include <alc.h>

#include <fstream>
#include <iostream>
#include <capo/capo.hpp>

namespace impl {
static constexpr int fail_code = 2;

[[noreturn]] void onError(ALenum error, int line) {
	std::cerr << "Misc OpenAL error: " << std::hex << error << " on line " << std::dec << line << std::endl;
	std::exit(fail_code);
}

void alCheck(int line = __builtin_LINE()) {
	if (auto err = alGetError(); err != AL_NO_ERROR) { onError(err, line); }
}

constexpr ALenum g_alFormats[] = {AL_FORMAT_MONO16, AL_FORMAT_STEREO16};
constexpr ALenum alFormat(capo::PCM::Format format) noexcept { return g_alFormats[static_cast<std::size_t>(format)]; }

class Instance final {
  public:
	Instance() noexcept : m_device(alcOpenDevice(nullptr)) {
		if (!m_device) {
			std::cerr << "Couldn't initialize OpenAL device" << std::endl;
		} else {
			m_context = alcCreateContext(m_device, nullptr);
			alcMakeContextCurrent(m_context);
		}
	}
	Instance(Instance&& rhs) noexcept { exchg(*this, rhs); }
	Instance& operator=(Instance rhs) noexcept { return (exchg(*this, rhs), *this); }
	~Instance() {
		if (valid()) {
			alcMakeContextCurrent(nullptr);
			alcDestroyContext(m_context);
			alcCloseDevice(m_device);
		}
	}

	bool valid() const noexcept { return m_device && m_context; }

	ALuint genBuffer() {
		ALuint ret{};
		if (valid()) {
			alGenBuffers(1, &ret);
			alCheck();
		}
		return ret;
	}

	ALuint genSource() {
		ALuint ret{};
		if (valid()) {
			alGenSources(1, &ret);
			alCheck();
		}
		return ret;
	}

	template <typename Cont>
	void bufferData(ALuint buffer, ALenum format, Cont const& data, ALsizei freq) {
		alBufferData(buffer, format, data.data(), static_cast<ALsizei>(data.size()) * sizeof(typename Cont::value_type), freq);
		alCheck();
	}

	void bufferData(ALuint buffer, capo::PCM const& pcm) { bufferData(buffer, alFormat(pcm.sampleFormat), pcm.samples, static_cast<ALsizei>(pcm.sampleRate)); }

	void setProp(ALuint source, ALenum param, ALint value) {
		alSourcei(source, param, value);
		alCheck();
	}

	void bind(ALuint source, ALuint buffer) { setProp(source, AL_BUFFER, buffer); }
	void loop(ALuint source, bool loop) { setProp(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE); }

  private:
	static void exchg(Instance& lhs, Instance& rhs) noexcept {
		std::swap(lhs.m_device, rhs.m_device);
		std::swap(lhs.m_context, rhs.m_context);
	}

	ALCdevice* m_device{};
	ALCcontext* m_context{};
};

int openAlTest(std::string const& wavPath) {
	auto pcm = capo::PCM::fromFile(wavPath);
	if (!pcm) { return impl::fail_code; }

	impl::Instance instance;
	if (!instance.valid()) { return impl::fail_code; }

	auto buffer = instance.genBuffer();
	instance.bufferData(buffer, *pcm);

	auto source = instance.genSource();
	instance.bind(source, buffer);
	instance.loop(source, true);

	alSourcePlay(source);

	getchar();
	return 0;
}
} // namespace impl

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Syntax: " << argv[0] << " <wav file path>" << std::endl;
		return impl::fail_code;
	}
	return impl::openAlTest(argv[1]);
}
