#include <capo/instance.hpp>
#include <capo/sound.hpp>
#include <capo/source.hpp>
#include <impl_al.hpp>
#include <AL/al.h>

namespace capo {
Source const Source::blank;

// TODO: Ensure validity of get/set externally from OpenAL checks

bool Source::bind(Sound const& sound) { return valid() ? m_instance->bind(sound, *this) : false; }
bool Source::unbind() { return valid() ? m_instance->unbind(*this) : false; }
Sound const& Source::bound() const noexcept { return valid() ? m_instance->bound(*this) : Sound::blank; }

bool Source::play() {
	if (valid()) {
		CAPO_CHKR(alSourcePlay(m_handle));
		return true;
	}
	return false;
}

bool Source::pause() {
	if (valid()) {
		CAPO_CHKR(alSourcePause(m_handle));
		return true;
	}
	return false;
}

bool Source::stop() {
	if (valid()) {
		CAPO_CHKR(alSourceStop(m_handle));
		return true;
	}
	return false;
}

bool Source::loop(bool loop) {
	if (valid()) { return detail::setSourceProp(m_handle, AL_LOOPING, loop ? AL_TRUE : AL_FALSE); }
	return false;
}

bool Source::seek(Time head) {
	if (valid()) { return detail::setSourceProp(m_handle, AL_SEC_OFFSET, static_cast<ALfloat>(head.count())); }
	return false;
}

bool Source::gain(float value) {
	if (valid()) {
		if (value < 0.0f) {
			detail::onError(Error::eInvalidValue);
			return false;
		}
		return detail::setSourceProp(m_handle, AL_GAIN, static_cast<ALfloat>(value));
	}
	return false;
}

bool Source::pitch(float multiplier) {
	if (valid()) { return detail::setSourceProp(m_handle, AL_PITCH, multiplier); }
	return false;
}

bool Source::position(Vec3 pos) {
	if (valid()) { return detail::setSourceProp(m_handle, AL_POSITION, pos); }
	return false;
}

bool Source::velocity(Vec3 vel) {
	if (valid()) { return detail::setSourceProp(m_handle, AL_VELOCITY, vel); }
	return false;
}

bool Source::max_distance(float r) {
	if (valid()) { return detail::setSourceProp(m_handle, AL_MAX_DISTANCE, r); }
	return false;
}

float Source::pitch() const { return valid() ? detail::getSourceProp<ALfloat>(m_handle, AL_PITCH) : 1.0f; }

Vec3 Source::position() const { return valid() ? detail::getSourceProp<Vec3>(m_handle, AL_POSITION) : Vec3{}; }
Vec3 Source::velocity() const { return valid() ? detail::getSourceProp<Vec3>(m_handle, AL_VELOCITY) : Vec3{}; }
float Source::max_distance() const { return valid() ? detail::getSourceProp<ALfloat>(m_handle, AL_MAX_DISTANCE) : 0.0f; }

bool Source::playing() const { return valid() && detail::getSourceProp<ALint>(m_handle, AL_SOURCE_STATE) == AL_PLAYING; }
Time Source::played() const { return valid() ? Time(detail::getSourceProp<ALfloat>(m_handle, AL_SEC_OFFSET)) : Time(); }
bool Source::looping() const { return valid() && detail::getSourceProp<ALint>(m_handle, AL_LOOPING) != 0; }
float Source::gain() const { return valid() ? detail::getSourceProp<ALfloat>(m_handle, AL_GAIN) : 0.0f; }
} // namespace capo
