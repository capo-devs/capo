#define DR_WAV_IMPLEMENTATION
#include <dr_libs/dr_wav.h>

#include <fstream>
#include <optional>
#include <capo/pcm.hpp>

namespace capo {
namespace {
class WAV {
  public:
	WAV(std::span<std::byte const> bytes) {
		if (!drwav_init_memory(&m_wav, bytes.data(), bytes.size(), nullptr)) { m_error = Error::eInvalidData; }
	}

	~WAV() {
		if (!m_error) { drwav_uninit(&m_wav); }
	}

	std::size_t read(std::vector<PCM::Sample>& out) { return drwav_read_pcm_frames_s16(&m_wav, m_wav.totalPCMFrameCount, out.data()); }

	drwav m_wav;
	std::optional<Error> m_error;
};

std::vector<std::byte> fileBytes(std::string const& path) {
	static_assert(sizeof(char) == sizeof(std::byte) && alignof(char) == alignof(std::byte));
	if (auto file = std::ifstream(path, std::ios::binary | std::ios::ate)) {
		file.unsetf(std::ios::skipws);
		auto const size = file.tellg();
		auto buf = std::vector<std::byte>((std::size_t)size);
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(buf.data()), (std::streamsize)size);
		return buf;
	}
	return {};
}
} // namespace

Result<PCM> PCM::fromFile(std::string const& path) {
	auto const bytes = fileBytes(path);
	if (bytes.empty()) { return Error::eIOError; }
	return fromMemory(bytes);
}

Result<PCM> PCM::fromMemory(std::span<std::byte const> wavBytes) {
	WAV wav(wavBytes); // can't use Result pattern here because an initialized drwav object contains and uses a pointer to its own address
	if (wav.m_error) {
		return *wav.m_error;
	} else if (wav.m_wav.channels < 1 || wav.m_wav.channels > 2) {
		return Error::eUnsupportedChannels;
	} else {
		auto const frames = wav.m_wav.totalPCMFrameCount;
		PCM ret;
		ret.sampleFormat = wav.m_wav.channels == 2 ? PCM::Format::eStereo16 : PCM::Format::eMono16;
		ret.sampleRate = static_cast<std::size_t>(wav.m_wav.sampleRate);
		ret.samples.resize(frames * wav.m_wav.channels);
		auto const read = wav.read(ret.samples);
		if (read < frames) { return Error::eUnexpectedEOF; }
		if (ret.samples.empty()) { return Error::eIOError; }
		return ret;
	}
	return Error::eUnknown;
}
} // namespace capo
