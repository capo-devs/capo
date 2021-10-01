#define DR_WAV_IMPLEMENTATION
#include <dr_libs/dr_wav.h>

#include <capo/pcm.hpp>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>

namespace capo {
namespace {
class WAV {
  public:
	WAV(std::span<std::byte const> bytes) noexcept {
		if (!drwav_init_memory(&m_wav, bytes.data(), bytes.size(), nullptr)) { m_error = Error::eInvalidData; }
	}

	WAV(std::string_view path) noexcept {
		if (!drwav_init_file(&m_wav, path.data(), nullptr)) { m_error = Error::eIOError; }
	}

	~WAV() noexcept {
		if (!m_error) { drwav_uninit(&m_wav); }
	}

	std::size_t read(std::span<PCM::Sample> out, std::size_t count) noexcept { return drwav_read_pcm_frames_s16(&m_wav, count, out.data()); }
	std::size_t read(std::span<PCM::Sample> out) noexcept { return read(out, m_wav.totalPCMFrameCount); }

	drwav m_wav;
	std::optional<Error> m_error;
};

enum class FileFormat { eUnknown, eWav };

[[maybe_unused]] FileFormat detect(std::filesystem::path const& path) noexcept {
	if (path.has_extension()) {
		auto ext = path.extension().string();
		for (auto& ch : ext) { ch = std::tolower(static_cast<unsigned char>(ch)); }
		if (ext == ".wav") { return FileFormat::eWav; }
	}
	return FileFormat::eUnknown;
}

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

SampleMeta makeMeta(drwav const& wav) noexcept {
	return {
		.rate = static_cast<std::size_t>(wav.sampleRate),
		.format = wav.channels == 2 ? SampleFormat::eStereo16 : SampleFormat::eMono16,
		.channels = static_cast<std::size_t>(wav.channels),
	};
}
} // namespace

bool PCM::supported(SampleMeta const& meta) noexcept {
	if (meta.channels == 0 || meta.channels > max_channels_v) { return false; }
	return meta.rate > 0;
}

Result<PCM> PCM::fromFile(std::string const& path) {
	auto const bytes = fileBytes(path);
	if (bytes.empty()) { return Error::eIOError; }
	return fromMemory(bytes);
}

Result<PCM> PCM::fromMemory(std::span<std::byte const> wavBytes) {
	if (wavBytes.empty()) { return Error::eIOError; }
	WAV wav(wavBytes); // can't use Result pattern here because an initialized drwav object contains and uses a pointer to its own address
	if (wav.m_error) {
		return *wav.m_error;
	} else {
		PCM ret;
		ret.meta = makeMeta(wav.m_wav);
		if (!supported(ret.meta)) { return Error::eUnsupportedMetadata; }
		ret.samples.resize(ret.meta.sampleCount(wav.m_wav.totalPCMFrameCount));
		auto const read = wav.read(ret.samples);
		if (read < wav.m_wav.totalPCMFrameCount) { return Error::eUnexpectedEOF; }
		ret.size = utils::Size::make(ret.samples);
		return ret;
	}
	return Error::eUnknown;
}
} // namespace capo
