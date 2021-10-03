#pragma once
#include <capo/types.hpp>
#include <capo/utils/format_unit.hpp>

namespace capo {
enum class SampleFormat { eMono16, eStereo16 };
using SampleRate = std::size_t;

///
/// \brief Represents audio metadata
///
struct Metadata {
	static constexpr std::size_t max_channels_v = 2;

	SampleRate rate{};
	SampleFormat format{};
	std::size_t totalFrameCount{};

	constexpr Time length() const noexcept { return Time(float(totalFrameCount) / float(rate)); }
	constexpr utils::Rate sampleRate() const noexcept { return utils::Rate::make(rate); }

	static constexpr bool supported(std::size_t channels) noexcept { return channels > 0 && channels <= max_channels_v; }

	static constexpr std::size_t sampleCount(std::size_t pcmFrameCount, std::size_t channels) noexcept { return pcmFrameCount * channels; }
	static constexpr std::size_t channelCount(SampleFormat format) noexcept { return format == SampleFormat::eStereo16 ? 2 : 1; }
};
} // namespace capo
