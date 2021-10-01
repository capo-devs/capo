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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

namespace capo {
namespace {
constexpr SampleMeta makeMeta(drwav_uint32 sampleRate, drwav_uint16 channels, std::size_t totalPCMFrameCount) noexcept {
	auto ret = SampleMeta{
		.rate = static_cast<std::size_t>(sampleRate),
		.format = channels == 2 ? SampleFormat::eStereo16 : SampleFormat::eMono16,
		.channels = static_cast<std::size_t>(channels),
	};
	ret.length = ret.duration(ret.sampleCount(static_cast<std::size_t>(totalPCMFrameCount)));
	return ret;
}

template <typename T>
struct MemChecker {
	T const org;
	T* ptr;

	MemChecker(T& t) noexcept : org(t), ptr(&t) {}
	~MemChecker() { assert(std::memcmp(&org, ptr, sizeof(T)) == 0); }
};

class WAV {
  public:
	WAV(std::span<std::byte const> bytes) noexcept {
		if (!drwav_init_memory(&m_wav, bytes.data(), bytes.size(), nullptr)) {
			m_error = Error::eInvalidData;
		} else {
			m_totalPCMFrameCount = static_cast<std::size_t>(m_wav.totalPCMFrameCount);
			m_channels = static_cast<std::size_t>(m_wav.channels);
		}
	}

	WAV(std::string_view path) noexcept {
		if (!drwav_init_file(&m_wav, path.data(), nullptr)) {
			m_error = Error::eIOError;
		} else {
			m_totalPCMFrameCount = static_cast<std::size_t>(m_wav.totalPCMFrameCount);
			m_channels = static_cast<std::size_t>(m_wav.channels);
		}
	}

	~WAV() noexcept {
		if (!m_error) { drwav_uninit(&m_wav); }
	}

	std::size_t read(std::span<PCM::Sample> out, std::size_t count) noexcept { return drwav_read_pcm_frames_s16(&m_wav, count, out.data()); }
	std::size_t read(std::span<PCM::Sample> out) noexcept { return read(out, m_wav.totalPCMFrameCount); }

	SampleMeta meta() const noexcept { return makeMeta(m_wav.sampleRate, m_wav.channels, m_totalPCMFrameCount); }

	drwav m_wav;
	std::optional<Error> m_error;
	std::size_t m_totalPCMFrameCount{};
	std::size_t m_channels{};
};

class FLAC {
  public:
	FLAC(std::span<std::byte const> bytes) noexcept {
		if (m_flac = drflac_open_memory(bytes.data(), bytes.size(), nullptr); !m_flac) {
			m_error = Error::eInvalidData;
		} else {
			m_totalPCMFrameCount = static_cast<std::size_t>(m_flac->totalPCMFrameCount);
			m_channels = static_cast<std::size_t>(m_flac->channels);
		}
	}

	FLAC(std::string_view path) noexcept {
		if (m_flac = drflac_open_file(path.data(), nullptr); !m_flac) {
			m_error = Error::eIOError;
		} else {
			m_totalPCMFrameCount = static_cast<std::size_t>(m_flac->totalPCMFrameCount);
			m_channels = static_cast<std::size_t>(m_flac->channels);
		}
	}

	~FLAC() noexcept {
		if (!m_error) { drflac_close(m_flac); }
	}

	std::size_t read(std::span<PCM::Sample> out, std::size_t count) noexcept { return drflac_read_pcm_frames_s16(m_flac, count, out.data()); }
	std::size_t read(std::span<PCM::Sample> out) noexcept { return m_flac ? read(out, m_flac->totalPCMFrameCount) : 0; }

	SampleMeta meta() const noexcept { return m_flac ? makeMeta(m_flac->sampleRate, m_flac->channels, m_totalPCMFrameCount) : SampleMeta(); }

	drflac* m_flac;
	std::optional<Error> m_error;
	std::size_t m_totalPCMFrameCount{};
	std::size_t m_channels{};
};

class MP3 {
  public:
	MP3(std::span<std::byte const> bytes) noexcept {
		if (!drmp3_init_memory(&m_mp3, bytes.data(), bytes.size(), nullptr)) {
			m_error = Error::eInvalidData;
		} else {
			m_totalPCMFrameCount = static_cast<std::size_t>(drmp3_get_mp3_frame_count(&m_mp3));
			m_channels = static_cast<std::size_t>(m_mp3.channels);
		}
	}

	MP3(std::string_view path) noexcept {
		if (!drmp3_init_file(&m_mp3, path.data(), nullptr)) {
			m_error = Error::eIOError;
		} else {
			m_totalPCMFrameCount = static_cast<std::size_t>(drmp3_get_mp3_frame_count(&m_mp3));
			m_channels = static_cast<std::size_t>(m_mp3.channels);
		}
	}

	~MP3() noexcept {
		if (!m_error) { drmp3_uninit(&m_mp3); }
	}

	std::size_t read(std::span<PCM::Sample> out, std::size_t count) noexcept { return drmp3_read_pcm_frames_s16(&m_mp3, count, out.data()); }
	std::size_t read(std::span<PCM::Sample> out) noexcept { return read(out, drmp3_get_mp3_frame_count(&m_mp3)); }

	SampleMeta meta() const noexcept { return makeMeta(m_mp3.sampleRate, m_mp3.channels, m_totalPCMFrameCount); }

	drmp3 m_mp3;
	std::optional<Error> m_error;
	std::size_t m_totalPCMFrameCount{};
	std::size_t m_channels{};
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

		ret.samples.resize(ret.meta.sampleCount(f.m_totalPCMFrameCount));
		auto const read = f.read(ret.samples);
		if (read < f.m_totalPCMFrameCount) { return Error::eUnexpectedEOF; }
		ret.size = utils::Size::make(ret.samples);

		return ret;
	}
}

struct ExtFileFormat {
	std::string_view ext;
	FileFormat format;
};

/* clang-format off */
static constexpr ExtFileFormat supportedFormats[] = {
	{".wav", FileFormat::eWav},
	{".flac", FileFormat::eFlac},
	{".mp3", FileFormat::eMp3}
};
/* clang-format on */

FileFormat formatFromFilename(std::string_view name) noexcept {
	for (auto const& [extension, format] : supportedFormats) {
		if (name.ends_with(extension)) { return format; }
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
} // namespace

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

	default: return Error::eUnknownFormat;
	}
}

struct PCM::Streamer::File {
	// one pinned optional per file type: can't use variant etc because can't move drwav
	std::optional<WAV> wav;
	std::optional<MP3> mp3;
	std::optional<FLAC> flac;
	std::string path;
	struct {
		SampleMeta meta;
		utils::Size total;
		std::size_t remain{};
		Time duration{};
	} shared;
	FileFormat format{};
	std::size_t channels = 1;

	Outcome open(std::string path) noexcept {
		auto func = [&](auto& out, FileFormat fmt) -> Outcome {
			out.emplace(path);
			if (out->m_error) { return *out->m_error; }
			auto const meta = out->meta();
			if (!meta.supported()) { return *out->m_error; }
			shared.meta = meta;
			format = fmt;
			channels = out->m_channels;
			shared.remain = channels * std::size_t(out->m_totalPCMFrameCount);
			shared.total = utils::Size::make(float(shared.remain * sizeof(PCM::Sample)));
			this->path = std::move(path);
			return Outcome::success();
		};

		auto fmt = detect(path);
		switch (fmt) {
		case FileFormat::eWav: return func(wav, fmt);
		case FileFormat::eMp3: return func(mp3, fmt);
		case FileFormat::eFlac: return func(flac, fmt);
		case FileFormat::eUnknown: return Error::eUnsupportedMetadata;
		case FileFormat::eCOUNT_: break;
		}
		return Error::eUnknown;
	}

	std::size_t read(std::span<PCM::Sample> out_samples) noexcept {
		if (shared.remain == 0) { return 0; }
		switch (format) {
		case FileFormat::eWav: {
			std::size_t combinedSize = out_samples.size() / channels;
			auto const read = wav->read(out_samples, combinedSize);
			auto const ret = read * channels;
			assert(shared.remain >= ret);
			shared.remain -= ret;
			return ret;
		}
		default: return 0;
		}
	}
};

// all SMFs need to be defined out-of-line for unique_ptr<incomplete_type> to compile
PCM::Streamer::Streamer() : m_impl(std::make_unique<File>()) {}
PCM::Streamer::Streamer(Streamer&&) noexcept = default;
PCM::Streamer& PCM::Streamer::operator=(Streamer&&) noexcept = default;
PCM::Streamer::Streamer(std::string path) : Streamer() { open(std::move(path)); }
PCM::Streamer::Streamer(PCM pcm) : Streamer() { preload(std::move(pcm)); }
PCM::Streamer::~Streamer() noexcept = default;

Outcome PCM::Streamer::open(std::string path) {
	m_preloaded.clear();
	return m_impl->open(std::move(path));
}

void PCM::Streamer::preload(PCM pcm) noexcept {
	m_preloaded = std::move(pcm.samples);
	m_impl->shared = {pcm.meta, utils::Size::make(m_preloaded), m_preloaded.size()};
}

Outcome PCM::Streamer::reopen() {
	if (!m_preloaded.empty()) {
		m_impl->shared.remain = 0;
		return Outcome::success();
	}
	if (!m_impl->path.empty()) { return m_impl->open(std::move(m_impl->path)); }
	return Error::eInvalidData;
}

bool PCM::Streamer::valid() const noexcept { return !m_preloaded.empty() || m_impl->format != FileFormat::eUnknown; }
SampleMeta const& PCM::Streamer::meta() const noexcept { return m_impl->shared.meta; }
utils::Size const& PCM::Streamer::size() const noexcept { return m_impl->shared.total; }
std::size_t PCM::Streamer::remain() const noexcept { return m_impl->shared.remain; }

std::size_t PCM::Streamer::read(std::span<PCM::Sample> out_samples) {
	if (!m_preloaded.empty()) {
		assert(m_impl->shared.remain <= m_preloaded.size());
		std::size_t const start = m_preloaded.size() - m_impl->shared.remain;
		if (start < m_preloaded.size()) {
			std::size_t const end = start + out_samples.size();
			std::size_t const ret = std::min(end - start, m_impl->shared.remain);
			std::span<Sample> frame(m_preloaded.data() + start, ret);
			std::copy(frame.begin(), frame.end(), out_samples.begin());
			m_impl->shared.remain -= ret;
			return ret;
		}
		return 0;
	} else {
		return m_impl->read(out_samples);
	}
}
} // namespace capo
