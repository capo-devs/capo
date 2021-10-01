#include <capo/instance.hpp>
#include <capo/pcm.hpp>
#include <capo/sound.hpp>
#include <capo/source.hpp>
#include <impl_al.hpp>
#include <ktl/kthread.hpp>

namespace capo {
Instance::Instance() {
#if defined(CAPO_USE_OPENAL)
	if (alcGetCurrentContext() != nullptr) {
		detail::onError(Error::eDuplicateInstance);
	} else {
		if (ALCdevice* device = alcOpenDevice(nullptr)) {
			if (ALCcontext* context = alcCreateContext(device, nullptr)) {
				m_device = device;
				m_context = context;
				CAPO_CHK(alcMakeContextCurrent(context));
			} else {
				detail::onError(Error::eContextFailure);
			}
		} else {
			detail::onError(Error::eDeviceFailure);
		}
	}
#endif
}

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
		fill(m_sources);
		detail::deleteSources(resources);
		// delete all buffers
		fill(m_sounds);
		detail::deleteBuffers(resources);
		// destroy context and close device
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(m_context.get<ALCcontext*>());
		alcCloseDevice(m_device.get<ALCdevice*>());
	}
#endif
}

bool Instance::valid() const noexcept { return use_openal_v ? m_device.contains<ALCdevice*>() && m_context.contains<ALCcontext*>() : valid_if_inactive_v; }

Sound const& Instance::makeSound(PCM const& pcm) {
	if (valid()) {
		auto buffer = detail::genBuffer(pcm.meta, pcm.samples);
		auto [it, _] = m_sounds.insert_or_assign(buffer, Sound(this, buffer, pcm.meta.length));
		return it->second;
	}
	return Sound::blank;
}

Source const& Instance::makeSource() {
	if (valid()) {
		auto source = detail::genSource();
		auto [it, _] = m_sources.insert_or_assign(source, Source(this, source));
		return it->second;
	}
	return Source::blank;
}

bool Instance::destroy(Sound const& sound) {
	if (valid() && sound.valid()) {
		// unbind all sources
		for (UID const src : m_bindings.map[sound.m_buffer]) { detail::setSourceProp(src, AL_BUFFER, 0); }
		// delete buffer
		ALuint const buf[] = {sound.m_buffer};
		detail::deleteBuffers(buf);
		// unmap buffer
		m_bindings.map.erase(sound.m_buffer);
		m_sounds.erase(sound.m_buffer);
		return true;
	}
	return false;
}

bool Instance::destroy(Source const& source) {
	if (valid() && source.valid()) {
		// delete source (implicitly unbinds buffer)
		ALuint const src[] = {source.m_handle};
		detail::deleteSources(src);
		// unmap source
		m_bindings.unbind(source);
		m_sources.erase(source.m_handle);
		return true;
	}
	return false;
}

Sound const& Instance::findSound(UID id) const noexcept {
	if (auto it = m_sounds.find(id); it != m_sounds.end()) { return it->second; }
	return Sound::blank;
}

Source const& Instance::findSource(UID id) const noexcept {
	if (auto it = m_sources.find(id); it != m_sources.end()) { return it->second; }
	return Source::blank;
}

bool Instance::bind(Sound const& sound, Source const& source) {
	if (valid() && source.valid() && sound.valid()) {
		if (source.playing()) { CAPO_CHKR(alSourceStop(source.m_handle)); }
		if (detail::setSourceProp(source.m_handle, AL_BUFFER, static_cast<ALint>(sound.m_buffer))) {
			m_bindings.bind(sound, source);
			return true;
		}
	}
	return false;
}

bool Instance::unbind(Source const& source) {
	if (valid() && source.valid()) {
		if (source.playing()) { CAPO_CHKR(alSourceStop(source.m_handle)); }
		if (detail::setSourceProp(source.m_handle, AL_BUFFER, 0)) {
			m_bindings.unbind(source);
			return true;
		}
	}
	return false;
}

Sound const& Instance::bound(Source const& source) const noexcept {
	if (valid()) {
		for (auto const& [buf, set] : m_bindings.map) {
			if (set.contains(source.m_handle)) {
				auto it = m_sounds.find(buf);
				return it == m_sounds.end() ? Sound::blank : it->second;
			}
		}
	}
	return Sound::blank;
}

void Instance::Bindings::bind(Sound const& sound, Source const& source) {
	unbind(source);
	map[sound.m_buffer].insert(source.m_handle);
}

void Instance::Bindings::unbind(Source const& source) {
	for (auto& [_, set] : map) { set.erase(source.m_handle); }
}
} // namespace capo
