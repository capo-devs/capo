#pragma once
#include <capo/types.hpp>
#include <capo/utils/id.hpp>

namespace capo {
class Instance;

///
/// \brief Lightweight handle to a ready-to-play audio clip, mounted on device-accessible memory; use Instance to create
///
class Sound {
  public:
	Sound() = default;
	static Sound const blank;

	bool valid() const noexcept { return m_instance && m_buffer != 0; }
	Time length() const noexcept { return m_length; }
	std::size_t size() const;

	bool operator==(Sound const& rhs) const noexcept { return m_instance == rhs.m_instance && m_buffer == rhs.m_buffer; }

  private:
	Sound(Instance* instance, UID buffer, Time length) noexcept : m_buffer(buffer), m_length(length), m_instance(instance) {}

	UID m_buffer;
	Time m_length{};
	Instance* m_instance{};

	friend class Instance;
	friend class Source;
};
} // namespace capo
