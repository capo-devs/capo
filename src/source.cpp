#include <capo/instance.hpp>
#include <capo/sound.hpp>
#include <capo/source.hpp>
#include <impl_al.hpp>

namespace capo {
Source const Source::blank;

bool Source::bind(Sound const& sound) { return valid() ? m_instance->bind(sound, *this) : false; }
bool Source::unbind() { return valid() ? m_instance->unbind(*this) : false; }
Sound const& Source::bound() const { return valid() ? m_instance->bound(*this) : Sound::blank; }

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

bool Source::seek(time head) {
	if (valid()) { return detail::setSourceProp(m_handle, AL_SEC_OFFSET, static_cast<ALfloat>(head.count())); }
	return false;
}

bool Source::gain(float value) {
	if (valid()) {
		assert(value >= 0.0f);
		return detail::setSourceProp(m_handle, AL_GAIN, static_cast<ALfloat>(value));
	}
	return false;
}

bool Source::playing() const { return valid() && detail::getSourceProp<ALint>(m_handle, AL_SOURCE_STATE) == AL_PLAYING; }
time Source::played() const { return valid() ? time(detail::getSourceProp<ALfloat>(m_handle, AL_SEC_OFFSET)) : time(); }
bool Source::looping() const { return valid() && detail::getSourceProp<ALint>(m_handle, AL_LOOPING) != 0; }
float Source::gain() const { return valid() ? detail::getSourceProp<ALfloat>(m_handle, AL_GAIN) : 0.0f; }
} // namespace capo
