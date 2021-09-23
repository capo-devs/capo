#pragma once
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

inline bool alCheck() {
	if (auto err = alGetError(); err != AL_NO_ERROR) {
		// TODO: User customization point
		std::cerr << "Error! " << std::hex << err << "]\n";
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
