#pragma once
#include <capo/types.hpp>
#include <capo/utils/format_size.hpp>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace capo {
enum class SampleFormat { eMono16, eStereo16 };

struct SampleMeta {
	std::size_t rate{};
	SampleFormat format{};
	std::uint8_t channels{};
};

///
/// \brief Uncompressed PCM data
///
/// Only 16-bit Mono/Stereo is supported
///
struct PCM {
	using Sample = std::int16_t;

	SampleMeta meta;
	std::vector<Sample> samples;
	utils::Size size{};

	static Result<PCM> fromFile(std::string const& wavPath);
	static Result<PCM> fromMemory(std::span<std::byte const> wavBytes);
};
} // namespace capo
