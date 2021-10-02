#include <capo/capo.hpp>
#include <ktl/str_format.hpp>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
static constexpr int fail_code = 2;

int musicTest(std::string const& path, float gain) {
	capo::Instance instance;
	if (!instance.valid()) {
		std::cerr << "Couldn't create valid instance." << std::endl;
		return fail_code;
	}

	capo::Music music(&instance);
	// auto pcm = capo::PCM::fromFile(path);
	// if (!pcm || !music.preload(std::move(*pcm))) {
	if (!music.open(path)) {
		std::cerr << "Failed to open " << path << std::endl;
		return fail_code;
	}

	music.gain(gain);
	if (!music.play()) {
		std::cerr << "Failed to play " << path << std::endl;
		return fail_code;
	}

	auto const& meta = music.meta();
	std::cout << ktl::format("{} info:\n\t{}s Length\n\t{} Channel(s)\n\t{}Hz Sample Rate\n\t{} Size\n", path, meta.length().count(),
							 meta.channelCount(meta.format), meta.rate, music.size());
	std::cout << ktl::format("Streaming {} at {.2f} gain\n", path, gain);

	int done{};

	std::cout << "  ____________________  " << std::flush;
	while (music.playing()) {
		std::this_thread::yield();

		int const progress = static_cast<int>(20 * music.played() / meta.length());
		if (progress > done) {
			std::cout << "\r  ";
			for (int i = 0; i < progress; i++) { std::cout << '='; }
			done = progress;
			std::cout << std::flush;
		}
	}
	assert(music.played() == capo::Time());
	std::cout << "=\ncapo v" << capo::version_v << " ^^\n";
	return 0;
}
} // namespace

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Syntax: " << argv[0] << " <wav file path> [gain]" << std::endl;
		return fail_code;
	}
	float const gain = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 1.0f;
	return musicTest(argv[1], gain > 0.0f ? gain : 1.0f);
}
