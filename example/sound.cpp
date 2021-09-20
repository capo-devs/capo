#include <al.h>
#include <alc.h>
#define DR_WAV_IMPLEMENTATION
#include <dr_libs/dr_wav.h>

#include <iostream>
#include <type_traits>
#include <vector>

namespace {
static constexpr int fail_code = 2;

[[noreturn]] void on_error(ALenum error, int line) {
	std::cerr << "Misc OpenAL error: " << std::hex << error << " on line " << std::dec << line << std::endl;
	std::exit(fail_code);
}

void al_check(int line = __builtin_LINE()) {
	if (auto err = alGetError(); err != AL_NO_ERROR) { on_error(err, line); }
}

class al_instance final {
  public:
	al_instance() noexcept : m_device(alcOpenDevice(nullptr)) {
		if (!m_device) {
			std::cerr << "Couldn't initialize OpenAL device" << std::endl;
		} else {
			m_context = alcCreateContext(m_device, nullptr);
			alcMakeContextCurrent(m_context);
		}
	}
	al_instance(al_instance&& rhs) noexcept { exchg(*this, rhs); }
	al_instance& operator=(al_instance rhs) noexcept { return (exchg(*this, rhs), *this); }
	~al_instance() {
		if (valid()) {
			alcMakeContextCurrent(nullptr);
			alcDestroyContext(m_context);
			alcCloseDevice(m_device);
		}
	}
	static void exchg(al_instance& lhs, al_instance& rhs) noexcept {
		std::swap(lhs.m_device, rhs.m_device);
		std::swap(lhs.m_context, rhs.m_context);
	}

	bool valid() const noexcept { return m_device && m_context; }
	explicit operator bool() const noexcept { return valid(); }

	ALuint gen_buffer() {
		ALuint ret;
		alGenBuffers(1, &ret);
		al_check();
		return ret;
	}

	ALuint gen_source() {
		ALuint ret;
		alGenSources(1, &ret);
		al_check();
		return ret;
	}

	template <typename Cont>
	void buffer_data(ALuint buffer, ALenum format, Cont const& data, ALsizei freq) {
		alBufferData(buffer, format, data.data(), static_cast<ALsizei>(data.size()) * sizeof(typename Cont::value_type), freq);
		al_check();
	}

	void set_prop(ALuint source, ALenum param, ALint value) {
		alSourcei(source, param, value);
		al_check();
	}

	void bind(ALuint source, ALuint buffer) { set_prop(source, AL_BUFFER, buffer); }
	void loop(ALuint source, bool loop) { set_prop(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE); }

  private:
	ALCdevice* m_device{};
	ALCcontext* m_context{};
};
} // namespace

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Syntax: " << argv[0] << " <wav file path>" << std::endl;
		return fail_code;
	}

	using Sample = int16_t;
	std::vector<Sample> sample_data;
	ALsizei sample_rate;
	ALenum sample_format;
	/* drwav */ {
		drwav wav;
		if (!drwav_init_file(&wav, argv[1], NULL)) {
			std::cerr << "Invalid file inputted" << std::endl;
			return fail_code;
		}
		sample_rate = static_cast<ALsizei>(wav.sampleRate);
		sample_data.resize((size_t)wav.totalPCMFrameCount * wav.channels);
		drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, sample_data.data());
		if (wav.channels == 1) {
			sample_format = AL_FORMAT_MONO16;
		} else if (wav.channels == 2) {
			sample_format = AL_FORMAT_STEREO16;
		} else {
			std::cerr << "Invalid sample channel count; Only mono and stereo supported" << std::endl;
			return fail_code;
		}
		drwav_uninit(&wav);
	}

	/* OpenAL */ {
		al_instance instance;
		if (!instance) { return fail_code; }

		auto buffer = instance.gen_buffer();
		instance.buffer_data(buffer, sample_format, sample_data, sample_rate);

		auto source = instance.gen_source();
		instance.bind(source, buffer);
		instance.loop(source, true);

		alSourcePlay(source);

		getchar();
	}
	return 0;
}
