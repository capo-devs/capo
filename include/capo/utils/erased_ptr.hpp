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

	template <typename T>
	constexpr bool contains() const noexcept {
		return m_hash == typeid(T).hash_code();
	}

	template <typename T>
	[[nodiscard]] constexpr T get() const noexcept {
		return contains<T>() ? reinterpret_cast<T>(m_ptr) : nullptr;
	}

  private:
	std::size_t m_hash{};
	void* m_ptr{};
};
} // namespace capo::detail
