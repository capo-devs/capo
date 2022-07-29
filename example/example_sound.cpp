#include <capo/capo.hpp>
#include <ktl/kformat.hpp>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <optional>
#include <thread>

namespace {
static constexpr int fail_code = 2;
// TODO: Add parameters for each one of these constants [Use clap?]
static constexpr float travel_circurference_radius = 2.0f;
static constexpr float travel_angular_speed = 2.0f;
static constexpr bool loop_audio = false;

// Uniform circular motion formulas
capo::Vec3 ucm_position(float angular_speed, float time, float radius) {
	return {std::cos(time * angular_speed) * radius, std::sin(time * angular_speed) * radius, 0};
}

capo::Vec3 ucm_velocity(float angular_speed, float time, float radius) {
	return {-radius * angular_speed * std::cos(time * angular_speed), radius * angular_speed * std::sin(time * angular_speed), 0};
}

bool sound_test(char const* wav_path, float gain, bool loop) {
	auto instance = capo::Instance::make();
	if (!instance->valid()) {
		std::cerr << "Couldn't create valid instance." << std::endl;
		return false;
	}

	auto pcm = capo::PCM::from_file(wav_path);
	if (!pcm) {
		switch (pcm.error()) {
		case capo::Error::eUnknownFormat:
			static_assert(static_cast<std::size_t>(capo::FileFormat::eCOUNT_) == 4, "Unhandled file format");
			std::cerr << "File format not supported. Currently supported formats: MP3, WAV and FLAC" << std::endl;
			break;

		case capo::Error::eIOError: std::cerr << "Couldn't open audio file. Check if the file exists and if it is readable." << std::endl; break;

		default: std::cerr << "Couldn't load audio file. (Error: " << static_cast<int>(pcm.error()) << ")" << std::endl; break;
		}

		return false;
	}
	capo::Sound sound = instance->make_sound(*pcm);

	capo::Source source = instance->make_source();
	source.gain(gain);
	if (!source.bind(sound)) {
		std::cerr << "Couldn't bind sound to source." << std::endl;
		return false;
	}
	source.loop(loop);
	source.play();

	auto const& meta = sound.meta();
	std::cout << ktl::kformat("{} info:\n\t{:.1f}s Length\n\t{} Channel(s)\n\t{} Sample Rate\n\t{} Size\n", wav_path, meta.length().count(),
							  pcm->meta.channel_count(meta.format), sound.sample_rate(), sound.size());
	std::cout << ktl::kformat("Playing {} once at {:.2f} gain\n", wav_path, gain);
	if (pcm->meta.format == capo::SampleFormat::eMono16) {
		std::cout << ktl::kformat("Travelling on a circurference around the listener; r={:.1f}, angular speed={:.1f}\n", travel_circurference_radius,
								  travel_angular_speed);
	} else {
		std::cout << "Warning: Input has more than one channel, positional audio is disabled" << std::endl;
	}

	int done{};
	auto start = std::chrono::high_resolution_clock::now();

	std::cout << "  ____________________  " << std::flush;
	while (source.state() == capo::State::ePlaying) {
		std::this_thread::yield();
		auto now = std::chrono::high_resolution_clock::now();
		auto time = std::chrono::duration<float>(now - start).count();

		capo::Vec3 position = ucm_position(travel_angular_speed, time, travel_circurference_radius);
		capo::Vec3 velocity = ucm_velocity(travel_angular_speed, time, travel_circurference_radius);

		source.position(position);
		source.velocity(velocity);

		int const progress = static_cast<int>(20 * source.played() / meta.length());
		if (progress > done) {
			std::cout << "\r  ";
			for (int i = 0; i < progress; i++) { std::cout << '='; }
			done = progress;
			std::cout << std::flush;
		}
	}
	assert(source.played() == capo::Time());
	std::cout << "=\ncapo v" << capo::version_v << " ^^\n";
	return true;
}
} // namespace

int main(int argc, char** argv) {
	if (argc < 2) {
		std::cerr << "Syntax: " << argv[0] << " <wav file path> [gain]" << std::endl;
		return fail_code;
	}
	float const gain = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 1.0f;
	bool successful = sound_test(argv[1], gain > 0.0f ? gain : 1.0f, loop_audio);

	if (successful) {
		return 0;
	} else {
		return fail_code;
	}
}
