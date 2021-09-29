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
	Sound const& bound() const;

	bool play() noexcept;
	bool pause() noexcept;
	bool stop() noexcept;
	bool seek(Time head) noexcept;

	bool gain(float value) noexcept;
	bool pitch(float multiplier) noexcept;
	bool loop(bool loop) noexcept;

	bool position(Vec3) noexcept;
	bool velocity(Vec3) noexcept;
	bool max_distance(float r) noexcept;

	bool playing() const noexcept;
	Time played() const noexcept;

	float gain() const noexcept;
	float pitch() const noexcept;
	bool looping() const noexcept;

	Vec3 position() const noexcept;
	Vec3 velocity() const noexcept;
	float max_distance() const noexcept;

	bool operator==(Source const& rhs) const noexcept { return m_instance == rhs.m_instance && m_handle == rhs.m_handle; }

  private:
	Source(Instance* instance, UID handle) noexcept : m_handle(handle), m_instance(instance) {}

	UID m_handle;
	Instance* m_instance{};

	friend class Instance;
};
} // namespace capo
