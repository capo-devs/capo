#include <capo/capo.hpp>
#include <ktl/kformat.hpp>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
static constexpr int fail_code = 2;

int music_test(char const* path, float const gain, int const rounds) {
	auto instance = capo::Instance::make();
	if (!instance->valid()) {
		std::cerr << "Couldn't create valid instance." << std::endl;
		return fail_code;
	}

	capo::Music music(instance.get());
	// auto pcm = capo::PCM::fromFile(path);
	// if (!pcm || !music.preload(std::move(*pcm))) {
	if (!music.open(path)) {
		std::cerr << "Failed to open " << path << std::endl;
		return fail_code;
	}

	auto const& meta = music.meta();
	music.gain(gain);
	if (!music.play()) {
		std::cerr << "Failed to play " << path << std::endl;
		return fail_code;
	}

	std::cout << ktl::kformat("{} info:\n\t{:.1f}s Length\n\t{} Channel(s)\n\t{} Sample Rate\n\t{} Size\n", path, meta.length().count(),
							  meta.channel_count(meta.format), music.sample_rate(), music.size());
	std::cout << ktl::kformat("Streaming {} at {:.2f} gain for {} round(s)\n", path, gain, rounds);

	for (int round{}; round < rounds; ++round) {
		int done{};
		std::cout << "\r  ____________________\r  " << std::flush;
		music.play();
		assert(music.state() == capo::State::ePlaying);
		while (music.state() == capo::State::ePlaying) {
			std::this_thread::yield();

			int const progress = static_cast<int>(20 * music.position() / meta.length());
			if (progress > done) {
				std::cout << "\r  ";
				for (int i = 0; i < progress; i++) { std::cout << '='; }
				done = progress;
				std::cout << std::flush;
			}
		}
	}
	assert(music.position() == capo::Time());
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
	int const rounds = argc > 3 ? std::atoi(argv[3]) : 2;
	return music_test(argv[1], gain > 0.0f ? gain : 1.0f, rounds >= 1 ? rounds : 1);
}
