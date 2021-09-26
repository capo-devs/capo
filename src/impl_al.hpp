#pragma once
#include <capo/error_handler.hpp>
#include <AL/al.h>
#include <AL/alc.h>

#include <iostream>

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

template <typename...>
constexpr bool always_false_v = false;

template <typename T, std::size_t N>
constexpr std::size_t arraySize(T const (&)[N]) noexcept {
	return N;
}

constexpr std::string_view g_errorNames[] = {
	"Unknown",		  "OpenAL Error",		"IO Error",		  "Invalid Data",	 "Unsupported Channels",
	"Unexpected EOF", "Duplicate Instance", "Device Failure", "Context Failure", "Invalid Value",
};

static_assert(arraySize(g_errorNames) == static_cast<std::size_t>(Error::eCOUNT_), "Error / g_errorNames size mismatch");

inline OnError g_onError = [](Error error) { std::cerr << "[capo] Error: " << g_errorNames[static_cast<std::size_t>(error)] << std::endl; };

inline void onError(Error error) {
	if (detail::g_onError) { detail::g_onError(error); }
}

inline bool alCheck() {
	if (auto err = alGetError(); err != AL_NO_ERROR) {
		onError(Error::eOpenALError);
		return false;
	}
	return true;
}

template <typename T>
bool setBufferProp(MU ALuint source, MU ALenum prop, MU T value) {
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
T getBufferProp(MU ALuint source, MU ALenum prop) {
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
bool setSourceProp(MU ALuint source, MU ALenum prop, MU T value) {
	if constexpr (std::is_same_v<T, ALint>) {
		CAPO_CHKR(alSourcei(source, prop, value));
	} else if constexpr (std::is_same_v<T, ALfloat>) {
		CAPO_CHKR(alSourcef(source, prop, value));
	} else {
		static_assert(always_false_v<T>, "Invalid type");
	}
	return true;
}

template <typename T>
T getSourceProp(MU ALuint source, MU ALenum prop) {
	T ret{};
	if constexpr (std::is_same_v<T, ALint>) {
		CAPO_CHKR(alGetSourcei(source, prop, &ret));
	} else if constexpr (std::is_same_v<T, ALfloat>) {
		CAPO_CHKR(alGetSourcef(source, prop, &ret));
	} else {
		static_assert(always_false_v<T>, "Invalid type");
	}
	return ret;
}

#undef MU
} // namespace capo::detail

namespace capo {
inline void setErrorCallback(OnError const& callback) { detail::g_onError = callback; }
} // namespace capo
