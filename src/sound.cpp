#include <capo/sound.hpp>
#include <impl_al.hpp>

namespace capo {
Sound const Sound::blank;

std::size_t Sound::size() const { return valid() ? static_cast<size_t>(detail::getBufferProp<ALsizei>(m_buffer, AL_SIZE)) : 0U; }
} // namespace capo
