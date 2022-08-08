#pragma once
#include <capo/sound.hpp>
#include <capo/source.hpp>
#include <capo/types.hpp>
#include <ktl/kunique_ptr.hpp>
#include <vector>

namespace capo {
struct PCM;

class Device {
  public:
	Device() = default;

	std::string_view name() const noexcept { return m_name; }

  private:
	Device(std::string_view name) noexcept : m_name(name) {}
	std::string_view m_name;
	friend class Instance;
};

class Instance {
	struct Tag {};

  public:
	static ktl::kunique_ptr<Instance> make(Device device = {});

	Instance(Tag) noexcept;
	~Instance();

	Instance& operator=(Instance&&) = delete;

	bool valid() const noexcept;
	explicit operator bool() const noexcept { return valid(); }

	Sound const& make_sound(PCM const& pcm);
	Source const& make_source();
	bool destroy(Sound const& sound);
	bool destroy(Source const& source);
	Sound const& find_sound(UID id) const noexcept;
	Source const& find_source(UID id) const noexcept;

	bool bind(Sound const& sound, Source const& source);
	bool unbind(Source const& source);
	Sound const& bound(Source const& source) const noexcept;

	static std::vector<Device> devices();
	Result<Device> device() const;

  private:
	struct Impl;
	ktl::kunique_ptr<Impl> m_impl{};
};
} // namespace capo
