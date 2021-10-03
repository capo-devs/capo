#pragma once
#include <capo/metadata.hpp>
#include <capo/types.hpp>
#include <capo/utils/format_unit.hpp>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace capo {
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

	Metadata meta;
	std::vector<Sample> samples;
	std::size_t bytes{};

	utils::Size size() const noexcept { return utils::Size::make(bytes); }

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

	Metadata const& meta() const noexcept;
	utils::Size size() const noexcept;
	utils::Rate rate() const noexcept;
	std::size_t remain() const noexcept;
	std::size_t read(std::span<Sample> out_samples);

  private:
	struct File;
	std::unique_ptr<File> m_impl;
	std::vector<Sample> m_preloaded;
};
} // namespace capo
