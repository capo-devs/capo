#include <capo/sound.hpp>
#include <impl_al.hpp>

namespace capo {
Sound const Sound::blank;

utils::Size Sound::size() const { return valid() ? utils::Size::make(detail::getBufferProp<ALint>(m_buffer, AL_SIZE)) : utils::Size(); }
utils::Rate Sound::sampleRate() const noexcept { return valid() ? m_meta.sampleRate() : utils::Rate(); }
} // namespace capo
