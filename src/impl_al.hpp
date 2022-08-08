#pragma once
#include <capo/error_handler.hpp>
#include <capo/pcm.hpp>
#include <capo/types.hpp>
#include <capo/utils/enum_array.hpp>
#if defined(CAPO_USE_OPENAL)
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <cassert>
#include <iostream>
#include <span>

#if !defined(CAPO_USE_OPENAL)
using ALchar = char;
using ALenum = int;
using ALint = int;
using ALuint = unsigned int;
using ALfloat = float;
using ALCdevice = void;
using ALCcontext = void;
constexpr auto AL_FALSE = 0;
constexpr auto AL_TRUE = 1;
constexpr auto AL_PITCH = 0x1003;
constexpr auto AL_POSITION = 0x1004;
constexpr auto AL_VELOCITY = 0x1006;
constexpr auto AL_MAX_DISTANCE = 0x1023;
constexpr auto AL_LOOPING = 0x1007;
constexpr auto AL_GAIN = 0x100A;
constexpr auto AL_SOURCE_STATE = 0x1010;
constexpr auto AL_PLAYING = 0x1012;
constexpr auto AL_PAUSED = 0x1013;
constexpr auto AL_BUFFERS_QUEUED = 0x1015;
constexpr auto AL_BUFFERS_PROCESSED = 0x1016;
constexpr auto AL_SEC_OFFSET = 0x1024;
constexpr auto AL_FORMAT_MONO16 = 0x1101;
constexpr auto AL_FORMAT_STEREO16 = 0x1103;
constexpr auto AL_SIZE = 0x2004;
constexpr auto AL_BUFFER = 0x1009;
#endif

namespace capo::detail {
using SamplesView = std::span<PCM::Sample const>;

constexpr ALenum g_alFormats[] = {AL_FORMAT_MONO16, AL_FORMAT_STEREO16};
constexpr ALenum al_format(capo::SampleFormat format) noexcept { return g_alFormats[static_cast<std::size_t>(format)]; }

template <typename...>
constexpr bool always_false_v = false;

constexpr utils::EnumStringView<Error> g_error_names = {
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

inline OnError g_on_error = [](Error error) { std::cerr << "[capo] Error: " << g_error_names[error] << std::endl; };

inline void on_error(Error error) noexcept(false) {
	if (detail::g_on_error) { detail::g_on_error(error); }
}

inline bool al_check() noexcept(false) {
#if defined(CAPO_USE_OPENAL)
	if (auto err = alGetError(); err != AL_NO_ERROR) {
		Error e = Error::eUnknown;
		switch (err) {
		case AL_INVALID_ENUM: e = Error::eOpenALInvalidEnum; break;
		case AL_INVALID_NAME: e = Error::eOpenALInvalidName; break;
		case AL_INVALID_OPERATION: e = Error::eOpenALInvalidOperation; break;
		case AL_INVALID_VALUE: e = Error::eOpenALInvalidValue; break;
		default: break;
		}
		on_error(e);
		return false;
	}
#endif
	return true;
}

template <typename F>
void parse_flat_string(ALchar const* const str, F&& per_string) noexcept(false) {
	std::size_t first = 0, last = 1;
	auto isEnd = [&]() { return str[first] == '\0' && str[first + 1] == '\0'; };
	while (!isEnd()) {
		for (; str[last] != '\0'; ++last)
			;
		per_string(std::string_view(str + first, last - first));
		first = last + 1;
		last = first + 1;
	}
}

#define MU [[maybe_unused]]
#define CAPO_DETAIL_ALCHK_RETXPR(expr, retxpr)                                                                                                                 \
	do {                                                                                                                                                       \
		expr;                                                                                                                                                  \
		if (!al_check()) { return retxpr }                                                                                                                     \
	} while (false)

#if defined(CAPO_USE_OPENAL)
#define CAPO_CHK(expr) CAPO_DETAIL_ALCHK_RETXPR(expr, ;)
#define CAPO_CHKR(expr) CAPO_DETAIL_ALCHK_RETXPR(expr, {};)
#else
#define CAPO_CHK(unused)
#define CAPO_CHKR(unused)
#endif

inline bool enumeration_extension_present() noexcept(false) {
#if defined(CAPO_USE_OPENAL)
	return alcIsExtensionPresent(nullptr, "ALC_ENUMERATION_EXT") == AL_TRUE;
#else
	return false;
#endif
}

inline std::string_view device_name(MU ALCdevice* device) noexcept(false) {
	std::string_view ret;
#if defined(CAPO_USE_OPENAL)
	if (enumeration_extension_present()) { ret = alcGetString(device, ALC_DEVICE_SPECIFIER); }
#endif
	return ret;
}

template <typename F>
inline void device_names(MU F&& perDevice) noexcept(false) {
#if defined(CAPO_USE_OPENAL)
	if (enumeration_extension_present()) { parse_flat_string(alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER), std::forward<F>(perDevice)); }
#endif
}

inline void make_context_current(MU ALCcontext* context) noexcept(false) {
#if defined(CAPO_USE_OPENAL)
	alcMakeContextCurrent(context);
	if (context) { al_check(); }
#endif
}

inline void close_device(MU ALCcontext* context, MU ALCdevice* device) noexcept(false) {
	al_check();
	make_context_current(nullptr);
#if defined(CAPO_USE_OPENAL)
	alcDestroyContext(context);
	alcCloseDevice(device);
#endif
}

template <typename T>
bool set_buffer_prop(MU ALuint source, MU ALenum prop, MU T value) noexcept(false) {
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
T get_buffer_prop(MU ALuint source, MU ALenum prop) noexcept(false) {
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
bool set_source_prop(MU ALuint source, MU ALenum prop, MU T value) noexcept(false) {
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
T get_source_prop(MU ALuint source, MU ALenum prop) noexcept(false) {
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

inline ALuint gen_buffer() noexcept(false) {
	ALuint ret{};
	CAPO_CHKR(alGenBuffers(1, &ret));
	return ret;
}

inline ALuint gen_source() noexcept(false) {
	ALuint ret{};
	CAPO_CHKR(alGenSources(1, &ret));
	return ret;
}

inline void delete_buffers(MU std::span<ALuint const> buffers) noexcept(false) {
	CAPO_CHK(alDeleteBuffers(static_cast<ALsizei>(buffers.size()), buffers.data()));
}

inline void delete_sources(MU std::span<ALuint const> sources) noexcept(false) {
	CAPO_CHK(alDeleteSources(static_cast<ALsizei>(sources.size()), sources.data()));
}

template <typename Cont>
void buffer_data(MU ALuint buffer, MU ALenum format, MU Cont const& data, MU std::size_t freq) noexcept(false) {
	CAPO_CHK(alBufferData(buffer, format, data.data(), static_cast<ALsizei>(data.size()) * sizeof(typename Cont::value_type), static_cast<ALsizei>(freq)));
}

inline void buffer_data(MU ALuint buffer, MU Metadata const& meta, MU SamplesView samples) noexcept(false) {
	buffer_data(buffer, al_format(meta.format), samples, meta.rate);
}

inline ALuint gen_buffer(MU Metadata const& meta, MU SamplesView samples) noexcept(false) {
	auto ret = gen_buffer();
	buffer_data(ret, meta, samples);
	return ret;
}

inline bool can_pop_buffer(MU ALuint source) noexcept(false) { return get_source_prop<ALint>(source, AL_BUFFERS_PROCESSED) > 0; }

inline ALuint pop_buffer(MU ALuint source) noexcept(false) {
	assert(can_pop_buffer(source));
	ALuint ret{};
	CAPO_CHKR(alSourceUnqueueBuffers(source, 1, &ret));
	return ret;
}

inline bool push_buffers(MU ALuint source, MU std::span<ALuint const> buffers) noexcept(false) {
	CAPO_CHKR(alSourceQueueBuffers(source, static_cast<ALsizei>(buffers.size()), buffers.data()));
	return true;
}

inline bool play_source(MU ALuint source) noexcept(false) {
	CAPO_CHKR(alSourcePlay(source));
	return true;
}

inline bool pause_source(MU ALuint source) noexcept(false) {
	CAPO_CHKR(alSourcePause(source));
	return true;
}

inline bool stop_source(MU ALuint source) noexcept(false) {
	CAPO_CHKR(alSourceStop(source));
	return true;
}

inline bool rewind_source(MU ALuint source) noexcept(false) {
	CAPO_CHKR(alSourceRewind(source));
	return true;
}

inline State source_state(MU ALuint source) noexcept(false) {
#if defined(CAPO_USE_OPENAL)
	auto const state = get_source_prop<ALint>(source, AL_SOURCE_STATE);
	switch (state) {
	case AL_INITIAL: return State::eIdle;
	case AL_PLAYING: return State::ePlaying;
	case AL_PAUSED: return State::ePaused;
	case AL_STOPPED: return State::eStopped;
	default: break;
	}
#endif
	return State::eUnknown;
}

constexpr float stream_progress(std::size_t samples, std::size_t remain) noexcept {
	return samples > 0 && remain <= samples ? static_cast<float>(samples - remain) / static_cast<float>(samples) : -1.0f;
}

#undef CAPO_DETAIL_ALCHK_RETXPR
#undef CAPO_CHK
#undef CAPO_CHKR
#undef MU
} // namespace capo::detail

inline void capo::set_error_callback(OnError callback) { detail::g_on_error = std::move(callback); }
