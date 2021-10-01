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

	bool valid() const noexcept { return use_openal_v ? m_instance && m_handle > 0 : valid_if_inactive_v; }
	bool bind(Sound const& sound);
	bool unbind();
	Sound const& bound() const noexcept;

	bool play();
	bool pause();
	bool stop();
	bool seek(Time head);

	bool gain(float value);
	bool pitch(float multiplier);
	bool loop(bool loop);

	bool position(Vec3);
	bool velocity(Vec3);
	bool max_distance(float r);

	bool playing() const;
	Time played() const;

	float gain() const;
	float pitch() const;
	bool looping() const;

	Vec3 position() const;
	Vec3 velocity() const;
	float max_distance() const;

	bool operator==(Source const& rhs) const noexcept { return m_instance == rhs.m_instance && m_handle == rhs.m_handle; }

  private:
	Source(Instance* instance, UID handle) noexcept : m_handle(handle), m_instance(instance) {}

	UID m_handle;
	Instance* m_instance{};

	friend class Instance;
};
} // namespace capo
