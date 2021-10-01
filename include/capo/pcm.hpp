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
	std::size_t channels{};

	constexpr std::size_t sampleCount(std::size_t pcmFrameCount) noexcept { return pcmFrameCount * channels; }
};

///
/// \brief Uncompressed PCM data
///
/// Only 16-bit Mono/Stereo is supported
///
struct PCM {
	using Sample = std::int16_t;

	static constexpr std::size_t max_channels_v = 2;

	SampleMeta meta;
	std::vector<Sample> samples;
	utils::Size size{};

	static bool supported(SampleMeta const& meta) noexcept;
	static Result<PCM> fromFile(std::string const& wavPath);
	static Result<PCM> fromMemory(std::span<std::byte const> wavBytes);
};
} // namespace capo
