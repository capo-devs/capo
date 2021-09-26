#pragma once
#include <ktl/expected.hpp>
#include <chrono>

namespace capo {
///
/// \brief Operational error
///
enum class Error { eUnknown, eIOError, eInvalidData, eUnsupportedChannels, eUnexpectedEOF, eDuplicateInstance, eDeviceFailure, eContextFailure, eInvalidValue };

///
/// \brief Operation result
///
/// Contains T if success, else Error
///
template <typename T>
using Result = ktl::expected<T, Error>;

using Time = std::chrono::duration<float>;
} // namespace capo
