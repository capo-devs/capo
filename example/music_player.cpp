#include <capo/capo.hpp>
#include <ktl/str_format.hpp>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

namespace {
static constexpr int fail_code = 2;

class Player {
  public:
	Player(ktl::not_null<capo::Instance*> instance, std::span<char const* const> paths) : m_music(instance) {
		std::copy(paths.begin(), paths.end(), std::back_inserter(m_paths));
		if (load()) {
			menu();
			while (input()) { menu(); }
		} else {
			throw std::runtime_error(std::string("Failed to load ") + paths[0]);
		}
	}

  private:
	std::string_view status() const {
		if (m_music.playing()) {
			return "PLAYING";
		} else if (m_music.paused()) {
			return "PAUSED";
		} else {
			return "STOPPED";
		}
	}
	void menu() {
		std::cout << '\n' << m_paths[m_idx] << " [" << std::fixed << std::setprecision(2) << m_music.gain() << " gain] [" << m_music.position().count() << "s]";
		std::cout << "\n == " << status() << " ==";
		if (m_paths.size() > 1) { std::cout << " [" << m_idx + 1 << '/' << m_paths.size() << ']'; }
		std::cout << "\n  [t/g] <value>\t: seek to seconds / set gain";
		if (m_music.playing()) {
			std::cout << "\n  [p/s]\t\t: pause / stop";
		} else {
			std::cout << "\n  [p]\t\t: play";
		}
		if (m_paths.size() > 1) { std::cout << "\n  [</>]\t\t: previous / next"; }
		std::cout << "\n  [q]\t\t: quit\n  [?]\t\t: refresh";
	}

	bool input() {
		std::cout << "\n: ";
		char ch;
		std::cin >> ch;
		switch (ch) {
		case 't': {
			float time;
			std::cin >> time;
			if (!m_music.seek(capo::Time(time))) { std::cerr << "\nseek fail!\n"; }
			break;
		}
		case 's': m_music.stop(); break;
		case 'p': {
			if (m_music.playing()) {
				m_music.pause();
			} else {
				m_music.play();
			}
			break;
		}
		case 'g': {
			float gain;
			std::cin >> gain;
			if (!m_music.gain(gain)) { std::cerr << "\ngain fail!\n"; }
			break;
		}
		case '>': advance(&Player::next); break;
		case '<': advance(&Player::prev); break;
		case 'q': return false;
		default: break;
		}
		std::cin.clear();
		std::cin.ignore();
		return true;
	}

	bool load() {
		auto pcm = capo::PCM::fromFile(std::string(m_paths[m_idx]));
		if (!pcm) {
			std::cerr << "Failed to load " << m_paths[m_idx] << std::endl;
			return false;
		}
		if (!m_music.preload(std::move(*pcm))) {
			std::cerr << "Failed to preload " << m_paths[m_idx] << std::endl;
			return false;
		}
		prelude();
		return true;
	}

	template <typename F>
	void advance(F f) {
		bool playing = m_music.playing();
		m_music.stop();
		bool success{};
		do {
			(this->*f)();
			success = load();
		} while (!success);
		if (playing) { m_music.play(); }
	}

	void prelude() {
		auto const& meta = m_music.meta();
		std::cout << ktl::format("\n  {}\n\t{.1f}s Length\n\t{} Channel(s)\n\t{} Sample Rate\n\t{} Size\n", m_paths[m_idx], meta.length().count(),
								 meta.channelCount(meta.format), m_music.sampleRate(), m_music.size());
	}

	void next() noexcept { m_idx = (m_idx + 1) % m_paths.size(); }
	void prev() noexcept { m_idx = (m_idx + m_paths.size() - 1) % m_paths.size(); }

	capo::Music m_music;
	std::vector<std::string> m_paths;
	std::size_t m_idx{};
};
} // namespace

int main(int argc, char const* const argv[]) {
	if (argc < 2) {
		std::cerr << "Syntax: " << argv[0] << " <file_path0> [file_path1 ...]" << std::endl;
		return fail_code;
	}
	capo::Instance instance;
	if (!instance.valid()) {
		std::cerr << "Failed to create instance" << std::endl;
		return fail_code;
	}
	try {
		Player player(&instance, std::span(argv + 1, std::size_t(argc - 1)));
	} catch (std::runtime_error const& e) {
		std::cerr << e.what() << std::endl;
		return fail_code;
	}
}
