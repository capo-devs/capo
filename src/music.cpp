#include <capo/instance.hpp>
#include <capo/music.hpp>
#include <impl_stream.hpp>

namespace capo {
using Clock = std::chrono::steady_clock;

struct Music::Impl {
	detail::StreamSource<> stream;

	bool play() { return stream.ready() && stream.play(); }
	bool pause() { return stream.ready() && detail::pauseSource(stream.source()); }
	bool stop() { return stream.ready() && stream.stop(); }

	bool gain(float value) const { return detail::setSourceProp(stream.source(), AL_GAIN, value); }
	float gain() const { return detail::getSourceProp<ALfloat>(stream.source(), AL_GAIN); }
	bool pitch(ALfloat value) const { return detail::setSourceProp(stream.source(), AL_PITCH, value); }
	float pitch() const { return detail::getSourceProp<ALfloat>(stream.source(), AL_PITCH); }
};

// all SMFs need to be defined out-of-line for unique_ptr<incomplete_type> to compile
Music::Music() : m_impl(std::make_unique<Impl>()) {}
Music::Music(Music&&) noexcept = default;
Music& Music::operator=(Music&&) noexcept = default;
Music::Music(ktl::not_null<Instance*> instance) : Music() { m_instance = instance; }
Music::~Music() = default;

bool Music::valid() const noexcept { return m_instance && m_instance->valid(); }
bool Music::ready() const { return valid() && m_impl->stream.ready(); }

Result<void> Music::open(std::string path) {
	if (valid()) {
		if (m_impl->stream.open(std::move(path))) { return Result<void>::success(); }
		return Error::eIOError;
	}
	return Error::eInvalidValue;
}

Result<void> Music::preload(PCM pcm) {
	if (valid()) {
		m_impl->stream.load(std::move(pcm));
		return Result<void>::success();
	}
	return Error::eInvalidValue;
}

bool Music::play() { return valid() && m_impl->play(); }
bool Music::pause() { return valid() && m_impl->pause(); }
bool Music::stop() { return valid() && m_impl->stop(); }
bool Music::gain(float value) { return valid() && m_impl->gain(value); }
float Music::gain() const { return valid() ? m_impl->gain() : -1.0f; }
bool Music::pitch(float value) { return valid() && m_impl->pitch(value); }
float Music::pitch() const { return valid() ? m_impl->pitch() : 0.0f; }
bool Music::loop(bool value) { return valid() ? (m_impl->stream.loop(value), true) : false; }
bool Music::looping() const { return valid() && m_impl->stream.looping(); }
Result<void> Music::seek(Time stamp) { return ready() && m_impl->stream.seek(stamp) ? Result<void>::success() : Error::eInvalidValue; }
Time Music::position() const { return m_impl->stream.position(); }

Metadata const& Music::meta() const {
	if (valid()) { return m_impl->stream.streamer().meta(); }
	static Metadata const fallback{};
	return fallback;
}

utils::Size Music::size() const { return valid() ? m_impl->stream.streamer().size() : utils::Size(); }
utils::Rate Music::sampleRate() const { return valid() ? m_impl->stream.streamer().rate() : utils::Rate(); }
State Music::state() const { return valid() ? detail::sourceState(m_impl->stream.source()) : State::eUnknown; }
} // namespace capo
