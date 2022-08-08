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
	std::size_t total_frame_count{};

	constexpr Time length() const noexcept { return rate > 0 ? Time(float(total_frame_count)) / float(rate) : Time(); }
	constexpr utils::Rate sample_rate() const noexcept { return utils::Rate::make(rate); }

	static constexpr bool supported(std::size_t channels) noexcept { return channels > 0 && channels <= max_channels_v; }

	static constexpr std::size_t sample_count(std::size_t pcm_frame_count, std::size_t channels) noexcept { return pcm_frame_count * channels; }
	static constexpr std::size_t channel_count(SampleFormat format) noexcept { return format == SampleFormat::eStereo16 ? 2 : 1; }
};
} // namespace capo
