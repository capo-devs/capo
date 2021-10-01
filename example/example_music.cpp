#include <capo/capo.hpp>
#include <ktl/str_format.hpp>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

namespace impl {
static constexpr int fail_code = 2;

int musicTest(std::string const& wavPath, float gain) {
	capo::Instance instance;
	if (!instance.valid()) {
		std::cerr << "Couldn't create valid instance." << std::endl;
		return impl::fail_code;
	}

	capo::Music music(&instance);
	if (!music.open(wavPath)) {
		std::cerr << "Failed to open " << wavPath << std::endl;
		return impl::fail_code;
	}

	music.gain(gain);
	if (!music.play()) {
		std::cerr << "Failed to play " << wavPath << std::endl;
		return impl::fail_code;
	}

	capo::Time const duration = music.meta().length;
	std::string_view const format = music.meta().format == capo::SampleFormat::eStereo16 ? "stereo" : "mono";
	std::cout << ktl::format("Streaming {} [{} {}] at {.2f} gain, length: {.1f}s\n", wavPath, music.size(), format, gain, duration.count());

	int done{};

	std::cout << "  ____________________  \n  " << std::flush;
	while (music.playing()) {
		std::this_thread::yield();

		int const progress = static_cast<int>(20 * music.played() / duration);
		if (progress > done) {
			std::cout << '=';
			done = progress;
			std::cout << std::flush;
		}
	}
	assert(music.played() == capo::Time());
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
	return impl::musicTest(argv[1], gain > 0.0f ? gain : 1.0f);
}
