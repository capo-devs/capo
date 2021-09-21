#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <vector>
#include <capo/types.hpp>

namespace capo {
///
/// \brief Uncompressed PCM data
///
/// Only 16-bit Mono/Stereo is supported
///
struct PCM {
	enum class Format { eMono16, eStereo16 };
	using Sample = std::int16_t;

	std::vector<Sample> samples;
	std::size_t sampleRate{};
	Format sampleFormat{};

	static Result<PCM> fromFile(std::string const& wavPath);
	static Result<PCM> fromMemory(std::span<std::byte const> wavBytes);
};
} // namespace capo
