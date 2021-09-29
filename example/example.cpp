#include <capo/capo.hpp>
#include <ktl/str_format.hpp>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>

namespace impl {
static constexpr int fail_code = 2;
// TODO: Add parameters for each one of these constants [Use clap?]
static constexpr float travel_circurference_radius = 2;
static constexpr float travel_angular_speed = 2;
static constexpr bool loop_audio = true;

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

	auto pcm = capo::PCM::fromFile(wavPath);
	if (!pcm) {
		std::cerr << "Couldn't load audio file." << std::endl;
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

	std::cout << ktl::format("Playing {} once at {.2f} gain, length: {.1f}s\n", wavPath, gain, sound.length().count());
	std::cout << ktl::format("Travelling on a circurference around the listener; r={.1f}, angular speed={.1f}\n", travel_circurference_radius,
							 travel_angular_speed);

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
