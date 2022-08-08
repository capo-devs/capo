#include <dr_libs/dr_common.h>
#include <dr_libs/dr_flac.h>
#include <dr_libs/dr_mp3.h>
#include <dr_libs/dr_wav.h>

#include <capo/pcm.hpp>
#include <capo/types.hpp>
#include <impl_al.hpp>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

namespace capo {
namespace {
template <typename T>
std::size_t pcm_frame_count(T& t) noexcept {
	return t.totalPCMFrameCount;
}
std::size_t pcm_frame_count(drmp3& t) noexcept { return drmp3_get_pcm_frame_count(&t); }

template <typename FinitFromMemory, typename FinitFromFile, typename Funinit, typename Fread, typename Fseek>
struct TFacade {
	FinitFromMemory const initFromMemory;
	FinitFromFile const initFromFile;
	Funinit const uninit;
	Fread const read;
	Fseek const seek;
};

template <typename... T>
TFacade(T...) -> TFacade<T...>;

template <typename TFormat>
constexpr auto make_facade() noexcept {
	if constexpr (std::is_same_v<TFormat, drwav>) {
		return TFacade{&drwav_init_memory, &drwav_init_file, &drwav_uninit, &drwav_read_pcm_frames_s16, &drwav_seek_to_pcm_frame};
	} else if constexpr (std::is_same_v<TFormat, drmp3>) {
		return TFacade{&drmp3_init_memory, &drmp3_init_file, &drmp3_uninit, &drmp3_read_pcm_frames_s16, &drmp3_seek_to_pcm_frame};
	} else if constexpr (std::is_same_v<TFormat, drflac>) {
		return TFacade{&drflac_open_memory, &drflac_open_file, &drflac_close, &drflac_read_pcm_frames_s16, &drflac_seek_to_pcm_frame};
	} else {
		static_assert(detail::always_false_v<TFormat>, "Invalid TFormat");
	}
}

template <typename TFormat>
class DrFormat {
  public:
	std::optional<Error> m_error{};
	Metadata m_meta{};
	std::size_t m_channels{};

	DrFormat(std::span<std::byte const> bytes) noexcept {
		if (m_format = facade().initFromMemory(bytes.data(), bytes.size(), nullptr); m_format) {
			set_meta_from_audio();
		} else {
			m_error = Error::eInvalidData;
		}
	}

	DrFormat(char const* path) noexcept {
		if (m_format = facade().initFromFile(path, nullptr); m_format) {
			set_meta_from_audio();
		} else {
			m_error = Error::eIOError;
		}
	}

	~DrFormat() noexcept {
		if (!m_error) { facade().uninit(m_format); }
	}

	std::size_t read(std::span<PCM::Sample> out, std::size_t count) noexcept { return facade().read(m_format, static_cast<std::uint64_t>(count), out.data()); }
	std::size_t read(std::span<PCM::Sample> out) noexcept { return read(out, m_meta.total_frame_count); }
	bool seek(std::size_t frameIndex) noexcept { return facade().seek(m_format, static_cast<std::uint64_t>(frameIndex)); }

  private:
	TFormat* m_format;

	static auto const& facade() noexcept {
		static constexpr auto f = make_facade<TFormat>();
		return f;
	}

	void set_meta_from_audio() noexcept {
		auto const frameCount = pcm_frame_count(*m_format);
		auto const channels = static_cast<std::size_t>(m_format->channels);
		auto const rate = static_cast<std::size_t>(m_format->sampleRate);
		m_meta = {
			.rate = rate,
			.format = channels == 2 ? SampleFormat::eStereo16 : SampleFormat::eMono16,
			.total_frame_count = frameCount,
		};
		m_channels = channels;
	}
};

using WAV = DrFormat<drwav>;
using FLAC = DrFormat<drflac>;
using MP3 = DrFormat<drmp3>;

template <typename TFormat>
Result<PCM> obtain_pcm(std::span<std::byte const> bytes) {
	TFormat f(bytes); // can't use Result pattern here because an initialized drwav object contains and uses a pointer to its own address
	if (f.m_error) {
		return *f.m_error;
	} else if (!Metadata::supported(f.m_channels) || f.m_meta.rate == 0) {
		return Error::eUnsupportedMetadata;
	} else {
		PCM ret;
		ret.meta = f.m_meta;
		ret.samples.resize(ret.meta.sample_count(f.m_meta.total_frame_count, Metadata::channel_count(f.m_meta.format)));
		auto const read = f.read(ret.samples);
		if (read < f.m_meta.total_frame_count) { return Error::eUnexpectedEOF; }
		ret.bytes = ret.samples.size() * sizeof(PCM::Sample);
		return ret;
	}
}

struct ExtFileFormat {
	std::string_view ext;
	FileFormat format;
};

/* clang-format off */
static constexpr ExtFileFormat supported_formats[] = {
	{".wav", FileFormat::eWav},
	{".flac", FileFormat::eFlac},
	{".mp3", FileFormat::eMp3}
};
/* clang-format on */

FileFormat format_from_filename(std::string_view name) noexcept {
	for (auto const& [extension, format] : supported_formats) {
		if (name.ends_with(extension)) { return format; }
	}
	return FileFormat::eUnknown;
}

std::vector<std::byte> file_bytes(char const* path) {
	static_assert(sizeof(char) == sizeof(std::byte) && alignof(char) == alignof(std::byte));
	if (auto file = std::ifstream(path, std::ios::binary | std::ios::ate)) {
		file.unsetf(std::ios::skipws);
		auto const size = file.tellg();
		auto buf = std::vector<std::byte>(static_cast<std::size_t>(size));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(size));
		return buf;
	}
	return {};
}

constexpr FileFormat operator+(FileFormat const a, int const b) { return static_cast<FileFormat>(static_cast<int>(a) + b); }
} // namespace

Result<PCM> PCM::from_file(char const* path, FileFormat format) {
	if (format == FileFormat::eUnknown) { format = format_from_filename(path); }
	return PCM::from_memory(file_bytes(path), format);
}

Result<PCM> PCM::from_memory(std::span<std::byte const> bytes, FileFormat format) {
	if (bytes.empty()) { return Error::eIOError; }

	static_assert(static_cast<int>(FileFormat::eCOUNT_) == 4, "Unhandled file format");
	auto try_load = [&](FileFormat const fmt) -> Result<PCM> {
		switch (fmt) {
		case FileFormat::eWav: return obtain_pcm<WAV>(bytes);
		case FileFormat::eFlac: return obtain_pcm<FLAC>(bytes);
		case FileFormat::eMp3: return obtain_pcm<MP3>(bytes);
		default: return Error::eUnknownFormat;
		}
	};

	// if format is specified, only attempt to load that
	if (format != FileFormat::eUnknown) { return try_load(format); }
	// otherwise attempt to load each supported format
	for (format = FileFormat::eUnknown + 1; format < FileFormat::eCOUNT_; format = format + 1) {
		if (auto pcm = try_load(format)) { return pcm; }
	}
	return Error::eUnknownFormat;
}

struct PCM::Streamer::File {
	// one pinned optional per file type: can't use variant etc because can't move drwav
	std::optional<WAV> wav;
	std::optional<MP3> mp3;
	std::optional<FLAC> flac;
	struct {
		Metadata meta;
		std::size_t bytes;
		std::size_t remain{};
	} shared;
	FileFormat format{};
	std::size_t channels = 1;

	Result<void> open(char const* path) noexcept {
		auto const fmt = format_from_filename(path);
		auto loadInto = [&](auto& out) -> Result<void> {
			out.emplace(path);
			if (out->m_error) { return *out->m_error; }
			if (!Metadata::supported(out->m_channels) || out->m_meta.rate == 0) { return Error::eUnsupportedMetadata; }
			shared.meta = out->m_meta;
			format = fmt;
			channels = out->m_channels;
			shared.remain = channels * std::size_t(out->m_meta.total_frame_count);
			shared.bytes = shared.remain * sizeof(PCM::Sample);
			return Result<void>::success();
		};
		switch (fmt) {
		case FileFormat::eWav: return loadInto(wav);
		case FileFormat::eMp3: return loadInto(mp3);
		case FileFormat::eFlac: return loadInto(flac);
		case FileFormat::eUnknown: return Error::eUnknownFormat;
		case FileFormat::eCOUNT_: return Error::eInvalidValue;
		}
		return Error::eUnknown;
	}

	std::size_t read(std::span<PCM::Sample> out_samples) noexcept {
		if (shared.remain == 0) { return 0; }
		auto readFrom = [&](auto& out) {
			std::size_t combinedSize = out_samples.size() / channels;
			auto const read = out->read(out_samples, combinedSize);
			auto const ret = read * channels;
			assert(shared.remain >= ret);
			shared.remain -= ret;
			return ret;
		};
		switch (format) {
		case FileFormat::eWav: return readFrom(wav);
		case FileFormat::eMp3: return readFrom(mp3);
		case FileFormat::eFlac: return readFrom(flac);
		default: return 0;
		}
	}

	bool seek(std::size_t frameIndex) noexcept {
		auto seekFrom = [&](auto& out) {
			if (out->seek(frameIndex)) {
				shared.remain = (out->m_meta.total_frame_count - frameIndex) * channels;
				return true;
			}
			return false;
		};
		switch (format) {
		case FileFormat::eWav: return seekFrom(wav);
		case FileFormat::eMp3: return seekFrom(mp3);
		case FileFormat::eFlac: return seekFrom(flac);
		default: break;
		}
		return false;
	}
};

// all SMFs need to be defined out-of-line for unique_ptr<incomplete_type> to compile
PCM::Streamer::Streamer() : m_impl(ktl::make_unique<File>()) {}
PCM::Streamer::Streamer(Streamer&&) noexcept = default;
PCM::Streamer& PCM::Streamer::operator=(Streamer&&) noexcept = default;
PCM::Streamer::Streamer(char const* path) : Streamer() { open(path); }
PCM::Streamer::Streamer(PCM pcm) : Streamer() { preload(std::move(pcm)); }
PCM::Streamer::~Streamer() noexcept = default;

Result<void> PCM::Streamer::open(char const* path) {
	m_preloaded.clear();
	return m_impl->open(path);
}

void PCM::Streamer::preload(PCM pcm) noexcept {
	m_preloaded = std::move(pcm.samples);
	m_impl->shared = {pcm.meta, m_preloaded.size() * sizeof(PCM::Sample), m_preloaded.size()};
}

bool PCM::Streamer::valid() const noexcept { return !m_preloaded.empty() || m_impl->format != FileFormat::eUnknown; }
Metadata const& PCM::Streamer::meta() const noexcept { return m_impl->shared.meta; }
utils::Size PCM::Streamer::size() const noexcept { return utils::Size::make(m_impl->shared.bytes); }
utils::Rate PCM::Streamer::rate() const noexcept { return m_impl->shared.meta.sample_rate(); }
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

Result<void> PCM::Streamer::seek(Time stamp) noexcept {
	float const ratio = valid() ? stamp / m_impl->shared.meta.length() : 0.0f;
	if (!m_preloaded.empty()) {
		m_impl->shared.remain = m_preloaded.size() - std::min(std::size_t(ratio * m_preloaded.size()), m_preloaded.size());
		return Result<void>::success();
	} else if (m_impl->format != FileFormat::eUnknown) {
		std::size_t const total = sample_count() / m_impl->channels;
		if (m_impl->seek(std::min(std::size_t(ratio * total), total))) { return Result<void>::success(); }
		return Error::eUnknown;
	}
	return Error::eInvalidData;
}

std::size_t PCM::Streamer::sample_count() const noexcept { return Metadata::sample_count(meta().total_frame_count, Metadata::channel_count(meta().format)); }
Time PCM::Streamer::position() const noexcept { return detail::stream_progress(sample_count(), remain()) * meta().length(); }
} // namespace capo
