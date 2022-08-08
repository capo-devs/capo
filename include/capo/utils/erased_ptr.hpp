#pragma once
#include <type_traits>
#include <typeindex>

namespace capo::detail {
///
/// \brief Type-erased pointer storage with type-safe retrieval
///
/// Useful for safely exposing private types across interface boundaries
/// Warning: const correctness of stored T and get<T>() must be managed by user
///
class ErasedPtr {
  public:
	constexpr ErasedPtr() = default;

	template <typename T>
		requires std::is_pointer_v<T>
	constexpr /*implicit*/ ErasedPtr(T ptr) noexcept : m_hash(typeid(T).hash_code()), m_ptr(ptr) {}

	constexpr ErasedPtr(ErasedPtr&& rhs) noexcept : ErasedPtr() { swap(*this, rhs); }
	constexpr ErasedPtr(ErasedPtr const& rhs) noexcept = default;
	constexpr ErasedPtr& operator=(ErasedPtr rhs) noexcept { return (swap(*this, rhs), *this); }

	template <typename T>
	constexpr bool contains() const noexcept {
		return m_hash == typeid(T).hash_code();
	}

	template <typename T>
	[[nodiscard]] constexpr T get() const noexcept {
		return contains<T>() ? reinterpret_cast<T>(m_ptr) : nullptr;
	}

	friend constexpr void swap(ErasedPtr& lhs, ErasedPtr& rhs) {
		std::swap(lhs.m_hash, rhs.m_hash);
		std::swap(lhs.m_ptr, rhs.m_ptr);
	}

  private:
	std::size_t m_hash{};
	void* m_ptr{};
};
} // namespace capo::detail
