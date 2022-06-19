#pragma once
#include <capo/sound.hpp>
#include <capo/source.hpp>
#include <capo/types.hpp>
#include <capo/utils/erased_ptr.hpp>
#include <capo/utils/id.hpp>
#include <unordered_map>
#include <unordered_set>
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
  public:
	Instance(Device device = {});
	~Instance();

	Instance(Instance&&) = default;
	Instance& operator=(Instance&&) = default;

	bool valid() const noexcept;

	Sound const& makeSound(PCM const& pcm);
	Source const& makeSource();
	bool destroy(Sound const& sound);
	bool destroy(Source const& source);
	Sound const& findSound(UID id) const noexcept;
	Source const& findSource(UID id) const noexcept;

	bool bind(Sound const& sound, Source const& source);
	bool unbind(Source const& source);
	Sound const& bound(Source const& source) const noexcept;

	static std::vector<Device> devices();
	Result<Device> device() const;

  private:
	struct Bindings {
		// Sound => Source
		std::unordered_map<UID::type, std::unordered_set<UID::type>> map;

		void bind(Sound const& sound, Source const& source);
		void unbind(Source const& source);
	};

	Bindings m_bindings;
	std::unordered_map<UID::type, Sound> m_sounds;
	std::unordered_map<UID::type, Source> m_sources;
	detail::ErasedPtr m_device;
	detail::ErasedPtr m_context;
};
} // namespace capo
