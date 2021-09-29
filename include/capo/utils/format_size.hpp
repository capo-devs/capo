#pragma once
#include <capo/utils/enum_array.hpp>
#include <iomanip>
#include <ostream>

namespace capo::utils {
struct Size {
	enum class Unit { eB, eKiB, eMiB, eGiB, eCOUNT_ };
	static constexpr EnumStringView<Unit> suffixes = {{"B", "KiB", "MiB", "GiB"}};

	float value{};
	Unit unit{};

	static constexpr Size make(float bytes) noexcept {
		constexpr float divisor = 1024.0f;
		Unit ret = Unit::eB;
		while (bytes > divisor && ret < Unit::eGiB) {
			++ret;
			bytes /= divisor;
		}
		return {bytes, ret};
	}

	template <typename Container>
	static constexpr Size make(Container const& container) noexcept {
		return make(static_cast<float>(container.size() * sizeof(typename Container::value_type)));
	}

	friend std::ostream& operator<<(std::ostream& out, Size const& size) { return out << std::setprecision(2) << size.value << suffixes[size.unit]; }
};
} // namespace capo::utils
