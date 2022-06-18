#pragma once
#include <capo/types.hpp>
#include <capo/utils/enum_array.hpp>
#include <ktl/kformat.hpp>
#include <iomanip>
#include <sstream>

namespace capo::utils {
template <typename Enum, int Divisor>
struct TEnumValue {
	static constexpr Enum last_v = static_cast<Enum>(static_cast<std::underlying_type_t<Enum>>(Enum::eCOUNT_) - 1);

	float value{};
	Enum unit{};

	template <typename T>
		requires std::is_arithmetic_v<T>
	static constexpr TEnumValue make(T const value) noexcept {
		auto val = static_cast<float>(value);
		Enum ret{};
		constexpr auto const divisor = static_cast<float>(Divisor);
		while (val > divisor && ret < last_v) {
			++ret;
			val /= divisor;
		}
		return {val, ret};
	}
};

enum class SizeUnit { eB, eKiB, eMiB, eGiB, eCOUNT_ };
enum class RateUnit { eHz, ekHz, eMHz, eGHz, eCOUNT_ };

static constexpr EnumStringView<SizeUnit> sizeSuffixes = {"B", "KiB", "MiB", "GiB"};
static constexpr EnumStringView<RateUnit> freqSuffixes = {"Hz", "kHz", "MHz", "GHz"};

using Size = TEnumValue<SizeUnit, 1024>;
using Rate = TEnumValue<RateUnit, 1000>;

struct Length {
	std::chrono::hours hours{};
	std::chrono::minutes minutes{};
	std::chrono::seconds seconds{};

	constexpr Length(Time time = {}) noexcept {
		hours = std::chrono::duration_cast<decltype(hours)>(time);
		minutes = std::chrono::duration_cast<decltype(minutes)>(time - hours);
		seconds = std::chrono::duration_cast<decltype(seconds)>(time - hours - minutes);
	}
};

inline std::string to_string(Size const& size) { return ktl::kformat("{:.2f}{}", size.value, sizeSuffixes[size.unit]); }
inline std::string to_string(Rate const& freq) { return ktl::kformat("{:.1f}{}", freq.value, freqSuffixes[freq.unit]); }
inline std::string to_string(Length const& length) {
	auto str = std::stringstream{};
	str.fill('0');
	str << length.hours.count() << ':' << std::setw(2) << length.minutes.count() << ':' << std::setw(2) << length.seconds.count();
	return str.str();
}
} // namespace capo::utils
