#pragma once
#include <capo/types.hpp>
#include <functional>

namespace capo {
using OnError = std::function<void(Error)>;
///
/// \brief Set custom error callback (or none)
///
void setErrorCallback(OnError const& callback);
} // namespace capo
