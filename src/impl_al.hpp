#pragma once
#include <capo/error_handler.hpp>
#include <capo/pcm.hpp>
#include <capo/types.hpp>
#include <capo/utils/enum_array.hpp>
#include <AL/al.h>
#include <AL/alc.h>
#include <cassert>
#include <iostream>
#include <span>

#define CAPO_DETAIL_ALCHK_RETXPR(expr, retxpr)                                                                                                                 \
	do {                                                                                                                                                       \
		expr;                                                                                                                                                  \
		if (!::capo::detail::alCheck()) { return retxpr }                                                                                                      \
	} while (false)

#if defined(CAPO_USE_OPENAL)
#define CAPO_CHK(expr) CAPO_DETAIL_ALCHK_RETXPR(expr, ;)
#define CAPO_CHKR(expr) CAPO_DETAIL_ALCHK_RETXPR(expr, {};)
#else
#define CAPO_CHK(unused)
#define CAPO_CHKR(unused)
#endif

namespace capo::detail {
#define MU [[maybe_unused]]

using SamplesView = std::span<PCM::Sample const>;

constexpr ALenum g_alFormats[] = {AL_FORMAT_MONO16, AL_FORMAT_STEREO16};
constexpr ALenum alFormat(capo::SampleFormat format) noexcept { return g_alFormats[static_cast<std::size_t>(format)]; }

template <typename...>
constexpr bool always_false_v = false;

constexpr utils::EnumStringView<Error> g_errorNames = {
	"Unknown",
	"OpenAL Error: Invalid Name",
	"OpenAL Error: Invalid Enum",
	"OpenAL Error: Invalid Value",
	"OpenAL Error: Invalid Operation",
	"IO Error",
	"Invalid Data",
	"Unsupported Metadata",
	"Unexpected EOF",
	"Duplicate Instance",
	"Device Failure",
	"Context Failure",
	"Invalid Value",
	"Unknown Format",
};

inline OnError g_onError = [](Error error) { std::cerr << "[capo] Error: " << g_errorNames[error] << std::endl; };

inline void onError(Error error) noexcept(false) {
	if (detail::g_onError) { detail::g_onError(error); }
}

inline bool alCheck() noexcept(false) {
	if (auto err = alGetError(); err != AL_NO_ERROR) {
		Error e = Error::eUnknown;
		switch (err) {
		case AL_INVALID_ENUM: e = Error::eOpenALInvalidEnum; break;
		case AL_INVALID_NAME: e = Error::eOpenALInvalidName; break;
		case AL_INVALID_OPERATION: e = Error::eOpenALInvalidOperation; break;
		case AL_INVALID_VALUE: e = Error::eOpenALInvalidValue; break;
		default: break;
		}
		onError(e);
		return false;
	}
	return true;
}

template <typename T>
bool setBufferProp(MU ALuint source, MU ALenum prop, MU T value) noexcept(false) {
	if constexpr (std::is_same_v<T, ALint>) {
		CAPO_CHKR(alBufferi(source, prop, value));
	} else if constexpr (std::is_same_v<T, ALfloat>) {
		CAPO_CHKR(alBufferf(source, prop, value));
	} else {
		static_assert(always_false_v<T>, "Invalid type");
	}
	return true;
}

template <typename T>
T getBufferProp(MU ALuint source, MU ALenum prop) noexcept(false) {
	T ret{};
	if constexpr (std::is_same_v<T, ALint>) {
		CAPO_CHKR(alGetBufferi(source, prop, &ret));
	} else if constexpr (std::is_same_v<T, ALfloat>) {
		CAPO_CHKR(alGetBufferf(source, prop, &ret));
	} else {
		static_assert(always_false_v<T>, "Invalid type");
	}
	return ret;
}

template <typename T>
bool setSourceProp(MU ALuint source, MU ALenum prop, MU T value) noexcept(false) {
	if constexpr (std::is_same_v<T, ALint>) {
		CAPO_CHKR(alSourcei(source, prop, value));
	} else if constexpr (std::is_same_v<T, ALfloat>) {
		CAPO_CHKR(alSourcef(source, prop, value));
	} else if constexpr (std::is_same_v<T, Vec3>) {
		CAPO_CHKR(alSource3f(source, prop, value.x, value.y, value.z));
	} else {
		static_assert(always_false_v<T>, "Invalid type");
	}
	return true;
}

template <typename T>
T getSourceProp(MU ALuint source, MU ALenum prop) noexcept(false) {
	T ret{};
	if constexpr (std::is_same_v<T, ALint>) {
		CAPO_CHKR(alGetSourcei(source, prop, &ret));
	} else if constexpr (std::is_same_v<T, ALfloat>) {
		CAPO_CHKR(alGetSourcef(source, prop, &ret));
	} else if constexpr (std::is_same_v<T, Vec3>) {
		CAPO_CHKR(alGetSource3f(source, prop, &ret.x, &ret.y, &ret.z));
	} else {
		static_assert(always_false_v<T>, "Invalid type");
	}
	return ret;
}

inline ALuint genBuffer() noexcept(false) {
	ALuint ret{};
	CAPO_CHKR(alGenBuffers(1, &ret));
	return ret;
}

inline ALuint genSource() noexcept(false) {
	ALuint ret{};
	CAPO_CHKR(alGenSources(1, &ret));
	return ret;
}

inline void deleteBuffers(MU std::span<ALuint const> buffers) noexcept(false) {
	CAPO_CHK(alDeleteBuffers(static_cast<ALsizei>(buffers.size()), buffers.data()));
}

inline void deleteSources(MU std::span<ALuint const> sources) noexcept(false) {
	CAPO_CHK(alDeleteSources(static_cast<ALsizei>(sources.size()), sources.data()));
}

template <typename Cont>
void bufferData(MU ALuint buffer, MU ALenum format, MU Cont const& data, MU std::size_t freq) noexcept(false) {
	CAPO_CHK(alBufferData(buffer, format, data.data(), static_cast<ALsizei>(data.size()) * sizeof(typename Cont::value_type), static_cast<ALsizei>(freq)));
}

inline void bufferData(MU ALuint buffer, MU Metadata const& meta, MU SamplesView samples) noexcept(false) {
	bufferData(buffer, alFormat(meta.format), samples, meta.rate);
}

inline ALuint genBuffer(MU Metadata const& meta, MU SamplesView samples) noexcept(false) {
	auto ret = genBuffer();
	bufferData(ret, meta, samples);
	return ret;
}

inline bool canPopBuffer(MU ALuint source) noexcept(false) { return getSourceProp<ALint>(source, AL_BUFFERS_PROCESSED) > 0; }

inline ALuint popBuffer(MU ALuint source) noexcept(false) {
	assert(canPopBuffer(source));
	ALuint ret{};
	CAPO_CHKR(alSourceUnqueueBuffers(source, 1, &ret));
	return ret;
}

inline bool pushBuffers(MU ALuint source, MU std::span<ALuint const> buffers) noexcept(false) {
	CAPO_CHKR(alSourceQueueBuffers(source, static_cast<ALsizei>(buffers.size()), buffers.data()));
	return true;
}

inline bool playSource(ALuint source) {
	CAPO_CHKR(alSourcePlay(source));
	return true;
}

inline bool pauseSource(ALuint source) {
	CAPO_CHKR(alSourcePause(source));
	return true;
}

inline bool stopSource(ALuint source) {
	CAPO_CHKR(alSourceStop(source));
	return true;
}

inline bool rewindSource(ALuint source) {
	CAPO_CHKR(alSourceRewind(source));
	return true;
}

#undef MU
} // namespace capo::detail

namespace capo {
inline void setErrorCallback(OnError const& callback) { detail::g_onError = callback; }
} // namespace capo
