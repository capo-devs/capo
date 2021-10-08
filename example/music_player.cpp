#include <capo/capo.hpp>
#include <ktl/kthread.hpp>
#include <ktl/str_format.hpp>
#include <cassert>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
static constexpr int fail_code = 2;

constexpr capo::utils::EnumArray<capo::State, std::string_view> g_stateNames = {
	"UNKNOWN", "IDLE", "PLAYING", "PAUSED", "STOPPED",
};

constexpr bool isLast(std::size_t index, std::size_t total) { return index + 1 == total; }

class Player {
  public:
	Player(ktl::not_null<capo::Instance*> instance) : m_music(instance) {}

	bool run(std::span<char const* const> paths) {
		if (!populate(paths)) { return false; }
		loadImpl();
		m_thread = ktl::kthread([this](ktl::kthread::stop_t stop) {
			while (!stop.stop_requested()) {
				if (playNext()) {
					next();
					play();
					menu();
				}
				ktl::kthread::yield();
			}
		});
		m_thread.m_join = ktl::kthread::policy::stop;
		menu();
		while (input()) { menu(); }
		return true;
	}

  private:
	enum class State { eStopped, ePlaying };

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

	void menu() const {
		std::scoped_lock lock(m_mutex);
		auto const length = capo::utils::Length(m_music.position());
		std::cout << '\n' << m_paths[m_idx] << " [" << std::fixed << std::setprecision(2) << m_music.gain() << " gain] [" << length << "]";
		std::cout << "\n == " << g_stateNames[m_music.state()] << " ==";
		if (m_paths.size() > 1) { std::cout << " [" << m_idx + 1 << '/' << m_paths.size() << ']'; }
		std::cout << "\n  [t/g] <value>\t: seek to seconds / set gain";
		if (m_state == State::ePlaying) {
			std::cout << "\n  [p/s]\t\t: pause / stop";
		} else {
			std::cout << "\n  [p]\t\t: play";
		}
		if (m_paths.size() > 1) { std::cout << "\n  [</>]\t\t: previous / next"; }
		std::cout << "\n  [q]\t\t: quit\n  [?]\t\t: refresh\n: " << std::flush;
	}

	bool input() {
		char ch;
		std::cin >> ch;
		switch (ch) {
		case 't': {
			float time;
			std::cin >> time;
			std::scoped_lock lock(m_mutex);
			if (!m_music.seek(capo::Time(time))) { std::cerr << "\nseek fail!\n"; }
			break;
		}
		case 's': {
			std::scoped_lock lock(m_mutex);
			m_music.stop();
			m_state = State::eStopped;
			break;
		}
		case 'p': {
			std::scoped_lock lock(m_mutex);
			if (m_music.state() == capo::State::ePlaying) {
				m_music.pause();
			} else {
				m_music.play();
			}
			m_state = State::ePlaying;
			break;
		}
		case 'g': {
			float gain;
			std::cin >> gain;
			std::scoped_lock lock(m_mutex);
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

	bool playNext() {
		std::unique_lock lock(m_mutex);
		auto const stopped = m_music.state() == capo::State::eStopped;
		auto const last = isLast(m_idx, m_paths.size());
		if (last && stopped) { m_state = State::eStopped; }
		return m_state == State::ePlaying && stopped && !last;
	}

	void play() {
		std::scoped_lock lock(m_mutex);
		m_music.play();
	}

	void next() {
		std::scoped_lock lock(m_mutex);
		m_idx = (m_idx + 1) % m_paths.size();
		advanceImpl();
	}

	void prev() {
		std::scoped_lock lock(m_mutex);
		m_idx = (m_idx + m_paths.size() - 1) % m_paths.size();
		advanceImpl();
	}

	void loadImpl() {
		m_music.preload(*capo::PCM::fromFile(std::string(m_paths[m_idx])));
		auto const& meta = m_music.meta();
		std::cout << ktl::format("\n  {}\n\t{.1f}s Length\n\t{} Channel(s)\n\t{} Sample Rate\n\t{} Size\n", m_paths[m_idx], meta.length().count(),
								 meta.channelCount(meta.format), m_music.sampleRate(), m_music.size());
	}

	void advanceImpl() {
		m_music.stop();
		loadImpl();
		if (m_state == State::ePlaying) { m_music.play(); }
	}

	capo::Music m_music;
	ktl::kthread m_thread;
	mutable std::mutex m_mutex;
	std::vector<std::string_view> m_paths;
	State m_state = State::eStopped;
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
