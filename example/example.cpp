#include <capo/capo.hpp>
#include <capo/types.hpp>
#include <ktl/str_format.hpp>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <thread>
#include <tuple>
#include <vector>

namespace impl {
static constexpr int fail_code = 2;
// TODO: Add parameters for each one of these constants [Use clap?]
static constexpr float travel_circurference_radius = 2.0f;
static constexpr float travel_angular_speed = 2.0f;
static constexpr bool loop_audio = false;
/* clang-format off */
static std::vector<std::tuple<std::string, capo::FileFormat>> supportedFormats{
	{".wav", capo::FileFormat::eWav},
	{".flac", capo::FileFormat::eFlac},
	{".mp3", capo::FileFormat::eMp3}
};
/* clang-format on */

std::optional<capo::FileFormat> formatFromFilename(std::string name) {
	for (auto const& [extension, format] : supportedFormats) {
		if (name.ends_with(extension)) { return format; }
	}
	return std::nullopt;
}

// Uniform circular motion formulas
capo::Vec3 UCMPosition(float angularSpeed, float time, float radius) {
	return {std::cos(time * angularSpeed) * radius, std::sin(time * angularSpeed) * radius, 0};
}

capo::Vec3 UCMVelocity(float angularSpeed, float time, float radius) {
	return {-radius * angularSpeed * std::cos(time * angularSpeed), radius * angularSpeed * std::sin(time * angularSpeed), 0};
}

int openAlTest(std::string const& wavPath, float gain, bool loop) {
	capo::Instance instance;
	if (!instance.valid()) {
		std::cerr << "Couldn't create valid instance." << std::endl;
		return impl::fail_code;
	}

	capo::FileFormat format;
	if (auto extension = formatFromFilename(wavPath)) {
		format = extension.value();
	} else {
		std::cerr << "File format not supported. Supported formats: ";
		for (auto const& [extension, _] : supportedFormats) { std::cerr << extension << " "; }
		std::cerr << std::endl;
		return fail_code;
	}

	auto pcm = capo::PCM::fromFile(wavPath, format);
	if (!pcm) {
		std::cerr << "Couldn't load audio file. (Error: " << static_cast<int>(pcm.error()) << ")" << std::endl;
		return impl::fail_code;
	}
	capo::Sound sound = instance.makeSound(*pcm);

	capo::Source source = instance.makeSource();
	source.gain(gain);
	if (!source.bind(sound)) {
		std::cerr << "Couldn't bind sound to source." << std::endl;
		return impl::fail_code;
	}
	source.loop(loop);
	source.play();

	// FIXME ktl::format does not display values correctly (open issue in repo with minimal example)
	std::cout << ktl::format("{} info:\n\t{.2f}s Length\n\t{} Channels\n\t{}Hz Sample Rate\n\t{} PCM Size\n", wavPath, sound.length().count(), pcm->meta.channels,
							 pcm->meta.rate , pcm->size);
	std::cout << ktl::format("Playing {} once at {.2f} gain\n", wavPath, gain);
	if (pcm->meta.channels == 1) {
		std::cout << ktl::format("Travelling on a circurference around the listener; r={.1f}, angular speed={.1f}\n", travel_circurference_radius,
								 travel_angular_speed);
	} else {
		std::cout << "Warning: Input has more than one channel, positional audio is disabled" << std::endl;
	}

	int done{};
	auto start = std::chrono::high_resolution_clock::now();

	std::cout << "  ____________________  " << std::flush;
	while (source.playing()) {
		std::this_thread::yield();
		auto now = std::chrono::high_resolution_clock::now();
		auto time = std::chrono::duration<float>(now - start).count();

		capo::Vec3 position = UCMPosition(travel_angular_speed, time, travel_circurference_radius);
		capo::Vec3 velocity = UCMVelocity(travel_angular_speed, time, travel_circurference_radius);

		source.position(position);
		source.velocity(velocity);

		int const progress = static_cast<int>(20 * source.played() / sound.length());
		if (progress > done) {
			std::cout << "\r  ";
			for (int i = 0; i < progress; i++) { std::cout << '='; }
			done = progress;
			std::cout << std::flush;
		}
	}
	assert(source.played() == capo::Time());
	std::cout << "=\ncapo v" << capo::version_v << " ^^\n";
	return 0;
}
} // namespace impl

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Syntax: " << argv[0] << " <wav file path> [gain]" << std::endl;
		return impl::fail_code;
	}
	float const gain = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 1.0f;
	return impl::openAlTest(argv[1], gain > 0.0f ? gain : 1.0f, impl::loop_audio);
}
