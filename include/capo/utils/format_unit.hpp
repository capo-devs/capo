#pragma once
#include <capo/utils/enum_array.hpp>
#include <iomanip>
#include <ostream>

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

inline std::ostream& operator<<(std::ostream& out, Size const& size) { return out << std::setprecision(2) << size.value << sizeSuffixes[size.unit]; }
inline std::ostream& operator<<(std::ostream& out, Rate const& freq) { return out << std::setprecision(2) << freq.value << freqSuffixes[freq.unit]; }
} // namespace capo::utils
