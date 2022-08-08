#pragma once
#include <concepts>
#include <cstdint>
#include <utility>

namespace capo {
///
/// \brief Move-aware wrapper over an integral ID
///
template <std::integral T>
class ID {
  public:
	using type = T;
	static constexpr T zero_v = T{};

	constexpr /*implicit*/ ID(T id = T{}) noexcept : m_id(id) {}
	constexpr ID(ID&& rhs) noexcept : ID() { std::swap(m_id, rhs.m_id); }
	constexpr ID(ID const& rhs) noexcept : m_id(rhs.m_id) {}
	constexpr ID& operator=(ID rhs) noexcept { return (std::swap(m_id, rhs.m_id), *this); }

	template <typename U>
	constexpr ID(ID<U> const& rhs) noexcept : m_id(static_cast<T>(rhs.m_id)) {}

	constexpr T const& value() const noexcept { return m_id; }
	constexpr /*implicit*/ operator T() const noexcept { return value(); }

  private:
	T m_id{};
};

///
/// \brief Alias for ID<u32>
///
using UID = ID<std::uint32_t>;
} // namespace capo
