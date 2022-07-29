#include <capo/instance.hpp>
#include <capo/pcm.hpp>
#include <capo/sound.hpp>
#include <capo/source.hpp>
#include <impl_al.hpp>
#include <ktl/async/kthread.hpp>
#include <unordered_map>
#include <unordered_set>

namespace capo {
struct Instance::Impl {
	struct Bindings {
		// Sound => Source
		std::unordered_map<UID::type, std::unordered_set<UID::type>> map;

		void bind(Sound const& sound, Source const& source) {
			unbind(source);
			map[sound.m_buffer].insert(source.m_handle);
		}

		void unbind(Source const& source) {
			for (auto& [_, set] : map) { set.erase(source.m_handle); }
		}
	};

	Bindings bindings{};
	std::unordered_map<UID::type, Sound> sounds{};
	std::unordered_map<UID::type, Source> sources{};
	ALCdevice* device{};
	ALCcontext* context{};
};

ktl::kunique_ptr<Instance> Instance::make([[maybe_unused]] Device device) {
#if defined(CAPO_USE_OPENAL)
	if (alcGetCurrentContext() != nullptr) {
		detail::on_error(Error::eDuplicateInstance);
		return {};
	}
	ALCdevice* al_device = alcOpenDevice(device.m_name.data());
	if (!al_device) {
		detail::on_error(Error::eDeviceFailure);
		return {};
	}
	ALCcontext* context = alcCreateContext(al_device, nullptr);
	if (!context) {
		detail::on_error(Error::eContextFailure);
		return {};
	}

	auto ret = ktl::make_unique<Instance>(Tag{});
	ret->m_impl = ktl::make_unique<Impl>();
	ret->m_impl->device = al_device;
	ret->m_impl->context = context;
	detail::make_context_current(context);
	return ret;
#else
	return ktl::make_unique<Instance>(Tag{});
#endif
}

Instance::Instance(Tag) noexcept {}

Instance::~Instance() {
#if defined(CAPO_USE_OPENAL)
	if (valid()) {
		std::vector<ALuint> resources;
		auto fill = [&resources](auto const& map) {
			resources.clear();
			resources.reserve(map.size());
			for (auto const& [id, _] : map) { resources.push_back(id); }
		};
		// delete all sources, implicitly unbinding all buffers
		fill(m_impl->sources);
		detail::delete_sources(resources);
		// delete all buffers
		fill(m_impl->sounds);
		detail::delete_buffers(resources);
		// destroy context and close device
		detail::close_device(m_impl->context, m_impl->device);
	}
#endif
}

bool Instance::valid() const noexcept { return use_openal_v ? m_impl->device && m_impl->context : valid_if_inactive_v; }

Sound const& Instance::make_sound(PCM const& pcm) {
	if (valid()) {
		auto buffer = detail::gen_buffer(pcm.meta, pcm.samples);
		auto [it, _] = m_impl->sounds.insert_or_assign(buffer, Sound(this, buffer, pcm.meta));
		return it->second;
	}
	return Sound::blank;
}

Source const& Instance::make_source() {
	if (valid()) {
		auto source = detail::gen_source();
		auto [it, _] = m_impl->sources.insert_or_assign(source, Source(this, source));
		return it->second;
	}
	return Source::blank;
}

bool Instance::destroy(Sound const& sound) {
	if (valid() && sound.valid()) {
		// unbind all sources
		for (UID const src : m_impl->bindings.map[sound.m_buffer]) { detail::set_source_prop(src, AL_BUFFER, 0); }
		// delete buffer
		ALuint const buf[] = {sound.m_buffer};
		detail::delete_buffers(buf);
		// unmap buffer
		m_impl->bindings.map.erase(sound.m_buffer);
		m_impl->sounds.erase(sound.m_buffer);
		return true;
	}
	return false;
}

bool Instance::destroy(Source const& source) {
	if (valid() && source.valid()) {
		// delete source (implicitly unbinds buffer)
		ALuint const src[] = {source.m_handle};
		detail::delete_sources(src);
		// unmap source
		m_impl->bindings.unbind(source);
		m_impl->sources.erase(source.m_handle);
		return true;
	}
	return false;
}

Sound const& Instance::find_sound(UID id) const noexcept {
	if (auto it = m_impl->sounds.find(id); it != m_impl->sounds.end()) { return it->second; }
	return Sound::blank;
}

Source const& Instance::find_source(UID id) const noexcept {
	if (auto it = m_impl->sources.find(id); it != m_impl->sources.end()) { return it->second; }
	return Source::blank;
}

bool Instance::bind(Sound const& sound, Source const& source) {
	if (valid() && source.valid() && sound.valid()) {
		if (any_in(source.state(), State::ePlaying, State::ePaused)) { detail::stop_source(source.m_handle); }
		if (detail::set_source_prop(source.m_handle, AL_BUFFER, static_cast<ALint>(sound.m_buffer))) {
			m_impl->bindings.bind(sound, source);
			return true;
		}
	}
	return false;
}

bool Instance::unbind(Source const& source) {
	if (valid() && source.valid()) {
		if (any_in(source.state(), State::ePlaying, State::ePaused)) { detail::stop_source(source.m_handle); }
		if (detail::set_source_prop(source.m_handle, AL_BUFFER, 0)) {
			m_impl->bindings.unbind(source);
			return true;
		}
	}
	return false;
}

Sound const& Instance::bound(Source const& source) const noexcept {
	if (valid()) {
		for (auto const& [buf, set] : m_impl->bindings.map) {
			if (set.contains(source.m_handle)) {
				auto it = m_impl->sounds.find(buf);
				return it == m_impl->sounds.end() ? Sound::blank : it->second;
			}
		}
	}
	return Sound::blank;
}

std::vector<Device> Instance::devices() {
	std::vector<Device> ret;
	detail::device_names([&ret](std::string_view name) { ret.push_back(name); });
	return ret;
}

Result<Device> Instance::device() const {
	if (valid()) { return Device(detail::device_name(m_impl->device)); }
	return Error::eInvalidValue;
}
} // namespace capo
