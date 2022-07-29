#include <capo/sound.hpp>
#include <impl_al.hpp>

namespace capo {
Sound const Sound::blank;

utils::Size Sound::size() const { return valid() ? utils::Size::make(detail::get_buffer_prop<ALint>(m_buffer, AL_SIZE)) : utils::Size(); }
utils::Rate Sound::sample_rate() const noexcept { return valid() ? m_meta.sample_rate() : utils::Rate(); }
} // namespace capo
