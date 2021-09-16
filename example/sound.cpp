#include <alc.h>
#include <al.h>
#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>

#include <iostream>

int main() {
    drwav wav;
    if (!drwav_init_file(&wav, "sound.wav", NULL)) {
        return -1;
    }

    int16_t* pSampleData = (int16_t*)malloc((size_t)wav.totalPCMFrameCount * wav.channels * sizeof(int16_t));
    drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, pSampleData);

    // Initialization
    auto device = alcOpenDevice(nullptr);
    if(!device) {
        return -2;
    }

    auto context = alcCreateContext(device, nullptr);
    alcMakeContextCurrent(context);

    ALuint buffer;
    alGenBuffers(1, &buffer);
    if (auto error = alGetError(); error != AL_NO_ERROR) {  return -3; }

    alBufferData(buffer, AL_FORMAT_STEREO16, pSampleData, (size_t)wav.totalPCMFrameCount * wav.channels * sizeof(int16_t), wav.sampleRate);
    if (auto error = alGetError(); error != AL_NO_ERROR) {  return -3; }

    ALuint source;
    alGenSources(1, &source);
    if (auto error = alGetError(); error != AL_NO_ERROR) {  return -3; }

    alSourcei(source, AL_BUFFER, buffer);
    if (auto error = alGetError(); error != AL_NO_ERROR) {  return -3; }

    alSourcei(source, AL_LOOPING, AL_TRUE);
    if (auto error = alGetError(); error != AL_NO_ERROR) {  return -3; }

    alSourcePlay(source);

    getchar();

    drwav_uninit(&wav);
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    alcCloseDevice(device);
    return 0;
}