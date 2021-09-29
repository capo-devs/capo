#pragma once
#include <string_view>
#include <type_traits>

namespace capo::utils {
template <typename E, typename T, std::size_t N = std::size_t(E::eCOUNT_)>
	requires std::is_enum_v<E>
struct EnumArray {
	T t[N] = {};

	constexpr T const& operator[](E e) const noexcept { return t[static_cast<std::size_t>(e)]; }
};

template <typename E>
using EnumStringView = EnumArray<E, std::string_view>;

template <typename E>
	requires std::is_enum_v<E>
constexpr E& operator++(E& out) noexcept { return (out = static_cast<E>(static_cast<int>(out) + 1), out); }
template <typename E>
	requires std::is_enum_v<E>
constexpr E& operator--(E& out) noexcept { return (out = static_cast<E>(static_cast<int>(out) - 1), out); }
} // namespace capo::utils
