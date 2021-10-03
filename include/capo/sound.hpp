#pragma once
#include <capo/metadata.hpp>
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

	Metadata const& meta() const noexcept { return m_meta; }
	bool valid() const noexcept { return use_openal_v ? m_instance && m_buffer != 0 : valid_if_inactive_v; }
	utils::Size size() const;
	utils::Rate sampleRate() const noexcept;

	bool operator==(Sound const& rhs) const noexcept { return m_instance == rhs.m_instance && m_buffer == rhs.m_buffer; }

  private:
	Sound(Instance* instance, UID buffer, Metadata meta) noexcept : m_meta(meta), m_buffer(buffer), m_instance(instance) {}

	Metadata m_meta;
	UID m_buffer;
	Instance* m_instance{};

	friend class Instance;
	friend class Source;
};
} // namespace capo
