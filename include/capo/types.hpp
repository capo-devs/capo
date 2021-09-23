#pragma once
#include <ktl/expected.hpp>
#include <chrono>

namespace capo {
///
/// \brief Operational error
///
enum class Error { eUnknown, eIOError, eInvalidData, eUnsupportedChannels, eUnexpectedEOF, eDuplicateInstance, eDeviceFailure, eContextFailure, eInvalidValue };

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

using Time = std::chrono::duration<float>;
} // namespace capo
