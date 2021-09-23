#include <capo/capo.hpp>
#include <ktl/str_format.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

namespace impl {
static constexpr int fail_code = 2;

int openAlTest(std::string const& wavPath, float gain) {
	auto pcm = capo::PCM::fromFile(wavPath);
	if (!pcm) { return impl::fail_code; }
	capo::Instance instance;
	if (!instance.valid()) { return impl::fail_code; }
	capo::Sound sound = instance.makeSound(*pcm);
	std::cout << ktl::format("Playing {} once at {.2f} gain, length: {.1f}s\n", wavPath, gain, sound.length().count());
	capo::Source source = instance.makeSource();
	source.gain(gain);
	if (!source.bind(sound)) { return impl::fail_code; }
	source.loop(false);
	source.play();
	int done{};
	std::cout << "  ____________________\n  " << std::flush;
	while (source.playing()) {
		std::this_thread::yield();
		int const progress = static_cast<int>(20 * source.played() / sound.length());
		while (progress > done) {
			std::cout << '=' << std::flush;
			++done;
		}
	}
	assert(source.played() == capo::time());
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
	return impl::openAlTest(argv[1], gain > 0.0f ? gain : 1.0f);
}
