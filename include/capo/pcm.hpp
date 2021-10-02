#pragma once
#include <capo/types.hpp>
#include <capo/utils/format_size.hpp>
#include <span>
#include <string>
#include <vector>

namespace capo {
enum class SampleFormat { eMono16, eStereo16 };
using SampleRate = std::size_t;

struct SampleMeta {
	SampleRate rate{};
	SampleFormat format{};
	std::size_t totalFrameCount{};
	static constexpr std::size_t max_channels_v = 2;
	constexpr Time length() const noexcept { return Time(float(totalFrameCount) / float(rate)); }

	static constexpr bool supported(std::size_t channels) noexcept { return channels > 0 && channels <= max_channels_v; }

	static constexpr std::size_t sampleCount(std::size_t pcmFrameCount, std::size_t channels) noexcept { return pcmFrameCount * channels; }
	static constexpr std::size_t channelCount(SampleFormat format) noexcept { return format == SampleFormat::eStereo16 ? 2 : 1; }
};

enum class FileFormat { eUnknown, eWav, eMp3, eFlac, eCOUNT_ };

///
/// \brief Uncompressed PCM data
///
/// Only 16-bit Mono/Stereo is supported
///
struct PCM {
	using Sample = std::int16_t;
	class Streamer;

	static constexpr std::size_t max_channels_v = 2;

	SampleMeta meta;
	std::vector<Sample> samples;
	utils::Size size{};

	static Result<PCM> fromFile(std::string const& path, FileFormat format = FileFormat::eUnknown);

	static Result<PCM> fromMemory(std::span<std::byte const> bytes, FileFormat format);
};

class PCM::Streamer {
  public:
	Streamer();
	Streamer(std::string path);
	Streamer(PCM pcm);
	Streamer(Streamer&&) noexcept;
	Streamer& operator=(Streamer&&) noexcept;
	~Streamer() noexcept;

	Outcome open(std::string path);
	void preload(PCM pcm) noexcept;
	Outcome reopen();
	bool valid() const noexcept;
	explicit operator bool() const noexcept { return valid(); }

	SampleMeta const& meta() const noexcept;
	utils::Size const& size() const noexcept;
	std::size_t remain() const noexcept;
	std::size_t read(std::span<Sample> out_samples);

  private:
	struct File;
	std::unique_ptr<File> m_impl;
	std::vector<Sample> m_preloaded;
};
} // namespace capo
