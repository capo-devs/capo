#include <al.h>
#include <alc.h>
#define DR_WAV_IMPLEMENTATION
#include <dr_libs/dr_wav.h>

#include <iostream>
#include <vector>

int main(int argc, char** argv) {
	if (argc != 2) {
		std::cerr << "Syntax: " << argv[0] << " <wav file path>" << std::endl;
		return 2;
	}

	using Sample = int16_t;
	std::vector<Sample> sample_data;
	ALsizei sample_rate;
	ALenum sample_format;
	{
		drwav wav;
		if (!drwav_init_file(&wav, argv[1], NULL)) {
			std::cerr << "Invalid file inputted" << std::endl;
			return 2;
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
			return 2;
		}
		drwav_uninit(&wav);
	}

	// Initialization
	auto device = alcOpenDevice(nullptr);
	if (!device) {
		std::cerr << "Couldn't initialize OpenAL device" << std::endl;
		return 2;
	}

#define ERR_EXIT(error)                                                                                                                                        \
	{                                                                                                                                                          \
		std::cerr << "Misc OpenAL error: " << std::hex << error << " on line " << std::dec << __LINE__ << std::endl;                                           \
		return 2;                                                                                                                                              \
	}

	auto context = alcCreateContext(device, nullptr);
	alcMakeContextCurrent(context);

	ALuint buffer;
	alGenBuffers(1, &buffer);
	if (auto error = alGetError(); error != AL_NO_ERROR) { ERR_EXIT(error); }

	alBufferData(buffer, sample_format, sample_data.data(), static_cast<ALsizei>(sample_data.size()) * sizeof(Sample), sample_rate);
	if (auto error = alGetError(); error != AL_NO_ERROR) { ERR_EXIT(error); }

	ALuint source;
	alGenSources(1, &source);
	if (auto error = alGetError(); error != AL_NO_ERROR) { ERR_EXIT(error); }

	alSourcei(source, AL_BUFFER, buffer);
	if (auto error = alGetError(); error != AL_NO_ERROR) { ERR_EXIT(error); }

	alSourcei(source, AL_LOOPING, AL_TRUE);
	if (auto error = alGetError(); error != AL_NO_ERROR) { ERR_EXIT(error); }

	alSourcePlay(source);

	getchar();

	alcMakeContextCurrent(nullptr);
	alcDestroyContext(context);
	alcCloseDevice(device);
	return 0;
}
