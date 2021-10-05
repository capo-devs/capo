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

bool Source::play() { return valid() && detail::playSource(m_handle); }
bool Source::pause() { return valid() && detail::pauseSource(m_handle); }
bool Source::stop() { return valid() && detail::stopSource(m_handle); }

bool Source::loop(bool loop) { return valid() && detail::setSourceProp(m_handle, AL_LOOPING, loop ? AL_TRUE : AL_FALSE); }
bool Source::seek(Time head) { return valid() && detail::setSourceProp(m_handle, AL_SEC_OFFSET, static_cast<ALfloat>(head.count())); }
bool Source::gain(float value) { return value >= 0.0f && valid() && detail::setSourceProp(m_handle, AL_GAIN, static_cast<ALfloat>(value)); }

bool Source::pitch(float multiplier) { return valid() && detail::setSourceProp(m_handle, AL_PITCH, multiplier); }
bool Source::position(Vec3 pos) { return valid() && detail::setSourceProp(m_handle, AL_POSITION, pos); }
bool Source::velocity(Vec3 vel) { return valid() && detail::setSourceProp(m_handle, AL_VELOCITY, vel); }
bool Source::maxDistance(float r) { return valid() && detail::setSourceProp(m_handle, AL_MAX_DISTANCE, r); }

float Source::pitch() const { return valid() ? detail::getSourceProp<ALfloat>(m_handle, AL_PITCH) : 1.0f; }

Vec3 Source::position() const { return valid() ? detail::getSourceProp<Vec3>(m_handle, AL_POSITION) : Vec3{}; }
Vec3 Source::velocity() const { return valid() ? detail::getSourceProp<Vec3>(m_handle, AL_VELOCITY) : Vec3{}; }
float Source::maxDistance() const { return valid() ? detail::getSourceProp<ALfloat>(m_handle, AL_MAX_DISTANCE) : -1.0f; }

bool Source::playing() const { return valid() && detail::getSourceProp<ALint>(m_handle, AL_SOURCE_STATE) == AL_PLAYING; }
Time Source::played() const { return valid() ? Time(detail::getSourceProp<ALfloat>(m_handle, AL_SEC_OFFSET)) : Time(); }
bool Source::looping() const { return valid() && detail::getSourceProp<ALint>(m_handle, AL_LOOPING) != 0; }
float Source::gain() const { return valid() ? detail::getSourceProp<ALfloat>(m_handle, AL_GAIN) : -1.0f; }
} // namespace capo
