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
	Player(ktl::not_null<capo::Instance*> instance) : m_music(instance) {}

	bool run(std::span<char const* const> paths) {
		if (!populate(paths)) { return false; }
		load();
		menu();
		while (input()) { menu(); }
		return true;
	}

  private:
	bool populate(std::span<char const* const> paths) {
		m_paths.reserve(paths.size());
		for (auto const path : paths) {
			if (!m_music.open(path)) {
				std::cerr << "Failed to open " << path << std::endl;
				continue;
			}
			m_paths.push_back(path);
		}
		return !m_paths.empty();
	}

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
		auto const length = capo::utils::Length(m_music.position());
		std::cout << '\n' << m_paths[m_idx] << " [" << std::fixed << std::setprecision(2) << m_music.gain() << " gain] [" << length << "]";
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
		case '>': next(); break;
		case '<': prev(); break;
		case 'q': return false;
		default: break;
		}
		std::cin.clear();
		std::cin.ignore();
		return true;
	}

	void load() {
		m_music.preload(*capo::PCM::fromFile(std::string(m_paths[m_idx])));
		prelude();
	}

	void advance() {
		bool const playing = m_music.playing();
		m_music.stop();
		load();
		if (playing) { m_music.play(); }
	}

	void prelude() {
		auto const& meta = m_music.meta();
		std::cout << ktl::format("\n  {}\n\t{.1f}s Length\n\t{} Channel(s)\n\t{} Sample Rate\n\t{} Size\n", m_paths[m_idx], meta.length().count(),
								 meta.channelCount(meta.format), m_music.sampleRate(), m_music.size());
	}

	void next() noexcept {
		m_idx = (m_idx + 1) % m_paths.size();
		advance();
	}

	void prev() noexcept {
		m_idx = (m_idx + m_paths.size() - 1) % m_paths.size();
		advance();
	}

	capo::Music m_music;
	std::vector<std::string_view> m_paths;
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
	Player player(&instance);
	if (!player.run(std::span(argv + 1, std::size_t(argc - 1)))) { return fail_code; }
}
