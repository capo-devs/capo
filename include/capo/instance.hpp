#pragma once
#include <capo/sound.hpp>
#include <capo/source.hpp>
#include <capo/types.hpp>
#include <capo/utils/erased_ptr.hpp>
#include <capo/utils/id.hpp>
#include <unordered_map>
#include <unordered_set>

namespace capo {
struct PCM;

class Instance {
  public:
	Instance();
	~Instance();

	bool valid() const noexcept;

	Sound const& makeSound(PCM const& pcm);
	Source const& makeSource();
	bool destroy(Sound const& sound);
	bool destroy(Source const& source);
	Sound const& findSound(UID id) const;
	Source const& findSource(UID id) const;

	bool bind(Sound const& sound, Source const& source);
	bool unbind(Source const& source);
	Sound const& bound(Source const& source) const;

  private:
	struct Bindings {
		// Buffer => Source
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
