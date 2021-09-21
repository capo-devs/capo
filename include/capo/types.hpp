#pragma once
#include <ktl/expected.hpp>

namespace capo {
///
/// \brief Operational error
///
enum class Error { eUnknown, eIOError, eInvalidData, eUnsupportedChannels, eUnexpectedEOF };

///
/// \brief Operation result
///
/// Contains T if success, else Error
///
template <typename T>
using Result = ktl::expected<T, Error>;
} // namespace capo
