#define DR_FLAC_IMPLEMENTATION
#include <dr_libs/dr_flac.h>
#define DR_MP3_IMPLEMENTATION
#include <dr_libs/dr_mp3.h>
#define DR_WAV_IMPLEMENTATION
#include <dr_libs/dr_wav.h>

#include <capo/pcm.hpp>
#include <capo/types.hpp>
#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <array>
#include <tuple>
#include <string_view>

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

	SampleMeta meta() const noexcept {
		return {
			.rate = static_cast<std::size_t>(m_wav.sampleRate),
			.format = m_wav.channels == 2 ? SampleFormat::eStereo16 : SampleFormat::eMono16,
			.channels = static_cast<std::size_t>(m_wav.channels),
		};
	}

	std::size_t totalPCMFrameCount() const noexcept { return m_wav.totalPCMFrameCount; }

	drwav m_wav;
	std::optional<Error> m_error;
};

class FLAC {
  public:
	FLAC(std::span<std::byte const> bytes) noexcept {
		if (auto m_flac = drflac_open_memory(bytes.data(), bytes.size(), nullptr); !m_flac) { m_error = Error::eInvalidData; }
	}

	FLAC(std::string_view path) noexcept {
		if (auto m_flac = drflac_open_file(path.data(), nullptr); !m_flac) { m_error = Error::eIOError; }
	}

	~FLAC() noexcept {
		if (!m_error) { drflac_close(m_flac); }
	}

	std::size_t read(std::span<PCM::Sample> out, std::size_t count) noexcept { return drflac_read_pcm_frames_s16(m_flac, count, out.data()); }
	std::size_t read(std::span<PCM::Sample> out) noexcept { return read(out, m_flac->totalPCMFrameCount); }

	SampleMeta meta() const noexcept {
		return {
			.rate = static_cast<std::size_t>(m_flac->sampleRate),
			.format = m_flac->channels == 2 ? SampleFormat::eStereo16 : SampleFormat::eMono16,
			.channels = static_cast<std::size_t>(m_flac->channels),
		};
	}

	std::size_t totalPCMFrameCount() const noexcept { return m_flac->totalPCMFrameCount; }

	drflac* m_flac;
	std::optional<Error> m_error;
};

class MP3 {
  public:
	MP3(std::span<std::byte const> bytes) noexcept {
		if (!drmp3_init_memory(&m_mp3, bytes.data(), bytes.size(), nullptr)) { m_error = Error::eInvalidData; }
	}

	MP3(std::string_view path) noexcept {
		if (!drmp3_init_file(&m_mp3, path.data(), nullptr)) { m_error = Error::eIOError; }
	}

	~MP3() noexcept {
		if (!m_error) { drmp3_uninit(&m_mp3); }
	}

	std::size_t read(std::span<PCM::Sample> out, std::size_t count) noexcept { return drmp3_read_pcm_frames_s16(&m_mp3, count, out.data()); }
	std::size_t read(std::span<PCM::Sample> out) noexcept { return read(out, drmp3_get_mp3_frame_count(&m_mp3)); }

	SampleMeta meta() const noexcept {
		return {
			.rate = static_cast<std::size_t>(m_mp3.sampleRate),
			.format = m_mp3.channels == 2 ? SampleFormat::eStereo16 : SampleFormat::eMono16,
			.channels = static_cast<std::size_t>(m_mp3.channels),
		};
	}

	std::size_t totalPCMFrameCount() noexcept { return drmp3_get_mp3_frame_count(&m_mp3); }

	drmp3 m_mp3;
	std::optional<Error> m_error;
};

[[maybe_unused]] FileFormat detect(std::filesystem::path const& path) noexcept {
	if (path.has_extension()) {
		auto ext = path.extension().string();
		for (auto& ch : ext) { ch = std::tolower(static_cast<unsigned char>(ch)); }
		if (ext == ".wav") { return FileFormat::eWav; }
	}
	return FileFormat::eUnknown;
}

template <typename TFormat>
Result<PCM> obtainPCM(std::span<std::byte const> bytes) {
	TFormat f(bytes); // can't use Result pattern here because an initialized drwav object contains and uses a pointer to its own address
	if (f.m_error) {
		return *f.m_error;
	} else {
		PCM ret;

		ret.meta = f.meta();
		if (!ret.meta.supported()) { return Error::eUnsupportedMetadata; }

		auto frameCount = f.totalPCMFrameCount();
		ret.samples.resize(ret.meta.sampleCount(frameCount));
		auto const read = f.read(ret.samples);
		if (read < frameCount) { return Error::eUnexpectedEOF; }
		ret.size = utils::Size::make(ret.samples);

		return ret;
	}
}

/* clang-format off */
static constexpr std::array<std::tuple<std::string_view, capo::FileFormat>, 3> supportedFormats{{
	{".wav", capo::FileFormat::eWav},
	{".flac", capo::FileFormat::eFlac},
	{".mp3", capo::FileFormat::eMp3}
}};
/* clang-format on */

capo::FileFormat formatFromFilename(std::string_view name) {
	for (auto const& [extension, format] : supportedFormats) {
		if (name.ends_with(extension)) { return format; }
	}
	return capo::FileFormat::eUnknown;
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

} // namespace

constexpr bool SampleMeta::supported() noexcept {
	if (channels == 0 || channels > max_channels_v) { return false; }
	return rate > 0;
}

Result<PCM> PCM::fromFile(std::string const& path, FileFormat format) {
	auto const bytes = fileBytes(path);
	if (bytes.empty()) { return Error::eIOError; }
	if (format == FileFormat::eUnknown) { format = formatFromFilename(path); }
	return PCM::fromMemory(bytes, format);
}

Result<PCM> PCM::fromMemory(std::span<std::byte const> bytes, FileFormat format) {
	if (bytes.empty()) { return Error::eIOError; }

	static_assert(static_cast<int>(FileFormat::eCOUNT_) == 4, "Unhandled file format");
	switch (format) {
	case FileFormat::eWav: return obtainPCM<WAV>(bytes);

	case FileFormat::eFlac: return obtainPCM<FLAC>(bytes);

	case FileFormat::eMp3: return obtainPCM<MP3>(bytes);

	default: return Error::eUnknown;
	}
}

} // namespace capo
