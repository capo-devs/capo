#pragma once
#include <capo/types.hpp>
#include <capo/utils/id.hpp>

namespace capo {
class Sound;
class Instance;

///
/// \brief Lightweight handle to an audio source in 3D space; use Instance to create
///
/// Can bind to Sound clips, play/pause/stop, loop, etc.
///
class Source {
  public:
	Source() = default;
	static Source const blank;

	bool valid() const noexcept { return m_instance && m_handle > 0; }
	bool bind(Sound const& sound);
	bool unbind();
	Sound const& bound() const;

	bool play();
	bool pause();
	bool stop();
	bool gain(float value);
	bool loop(bool loop);
	bool seek(time head);
	bool playing() const;
	time played() const;
	bool looping() const;
	float gain() const;

	bool operator==(Source const& rhs) const noexcept { return m_instance == rhs.m_instance && m_handle == rhs.m_handle; }

  private:
	Source(Instance* instance, UID handle) noexcept : m_handle(handle), m_instance(instance) {}

	UID m_handle;
	Instance* m_instance{};

	friend class Instance;
};
} // namespace capo
