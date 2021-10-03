#pragma once
#include <ktl/expected.hpp>
#include <chrono>

namespace capo {
///
/// \brief Operational error
///
enum class Error {
	eUnknown,
	eOpenALInvalidName,
	eOpenALInvalidEnum,
	eOpenALInvalidValue,
	eOpenALInvalidOperation,
	eIOError,
	eInvalidData,
	eUnsupportedMetadata,
	eUnexpectedEOF,
	eDuplicateInstance,
	eDeviceFailure,
	eContextFailure,
	eInvalidValue,
	eUnknownFormat,
	eCOUNT_,
};

constexpr bool use_openal_v =
#if defined(CAPO_USE_OPENAL)
	true;
#else
	false;
#endif

constexpr bool valid_if_inactive_v =
#if defined(CAPO_VALID_IF_INACTIVE)
	true;
#else
	false;
#endif

///
/// \brief Operation result
///
/// Contains T if success, else Error
///
template <typename T>
using Result = ktl::expected<T, Error>;

///
/// \brief Operation result
///
/// Models true if success, else Error
///
class Outcome {
	Error m_error{};
	bool m_value = true;

  public:
	static constexpr Outcome success() noexcept { return Outcome(); }

	constexpr Outcome() noexcept = default;
	constexpr Outcome(Error error) noexcept : m_error(error), m_value(false) {}

	constexpr bool has_value() const noexcept { return m_value; }
	constexpr explicit operator bool() const noexcept { return has_value(); }
	constexpr Error error() const noexcept { return m_error; }
};

using Time = std::chrono::duration<float>;

struct Vec3 {
	float x, y, z;
};
} // namespace capo
