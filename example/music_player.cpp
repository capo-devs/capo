#include <capo/capo.hpp>
#include <ktl/async/kthread.hpp>
#include <ktl/kformat.hpp>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <ktl/async/kmutex.hpp>
#include <atomic>
#include <future>

namespace {
using namespace std::chrono_literals;

static constexpr int fail_code = 2;

constexpr capo::utils::EnumArray<capo::State, std::string_view> g_stateNames = {
	"UNKNOWN", "IDLE", "PLAYING", "PAUSED", "STOPPED",
};

constexpr bool is_last(std::size_t index, std::size_t total) { return index + 1 == total; }

using Tracklist = std::vector<std::string>;

Tracklist valid_tracks(Tracklist tracklist, ktl::not_null<capo::Instance*> instance) {
	Tracklist ret;
	ret.reserve(tracklist.size());
	capo::Music music(instance);
	for (auto& track : tracklist) {
		if (!music.open(track.c_str())) {
			std::cerr << "Failed to open " << track << std::endl;
			continue;
		}
		ret.push_back(std::move(track));
	}
	return ret;
}

class Playlist {
  public:
	enum class Mode { eStream, ePreload };

	bool load(Tracklist tracklist, Mode mode) {
		if (tracklist.empty()) {
			std::cerr << "Failed to load any valid tracks";
			return false;
		}
		m_mode = mode;
		m_tracklist = std::move(tracklist);
		if (m_mode == Mode::ePreload) {
			m_current = load(m_tracklist[m_idx].c_str());
			load_cache();
		}
		return true;
	}

	std::size_t index() const noexcept { return m_idx; }
	std::size_t size() const noexcept { return m_tracklist.size(); }
	bool multi_track() const noexcept { return size() > 1; }
	bool is_last_track() const noexcept { return is_last(index(), size()); }
	std::string const& path() const noexcept { return m_tracklist[index()]; }
	capo::PCM const& pcm() const noexcept { return m_current; }
	Mode mode() const noexcept { return m_mode; }

	void next() {
		if (multi_track()) {
			m_idx = next_idx();
			if (m_mode == Mode::ePreload) {
				m_current = std::move(m_cache.next)();
				load_cache();
			}
		}
	}

	void prev() {
		if (multi_track()) {
			m_idx = prev_idx();
			if (m_mode == Mode::ePreload) {
				auto& cache = m_cache.prev.fence.valid() ? m_cache.prev : m_cache.next;
				m_current = std::move(cache)();
				load_cache();
			}
		}
	}

  private:
	struct Cache {
		std::future<void> fence;
		capo::PCM pcm;

		capo::PCM operator()() && {
			fence.wait();
			return std::move(pcm);
		}
	};

	static capo::PCM load(char const* path) { return *capo::PCM::from_file(path); }

	std::size_t next_idx() const noexcept { return (m_idx + 1) % m_tracklist.size(); }
	std::size_t prev_idx() const noexcept { return (m_idx + m_tracklist.size() - 1) % m_tracklist.size(); }

	void load_cache() {
		if (multi_track()) {
			if (m_cache.next.fence.wait_for(0s) == std::future_status::deferred) { m_cache.next.fence.wait(); }
			m_cache.next.fence = std::async([this]() { m_cache.next.pcm = load(m_tracklist[next_idx()].c_str()); });
		}
		if (m_tracklist.size() > 2) {
			if (m_cache.prev.fence.wait_for(0s) == std::future_status::deferred) { m_cache.prev.fence.wait(); }
			m_cache.prev.fence = std::async([this]() { m_cache.prev.pcm = load(m_tracklist[prev_idx()].c_str()); });
		}
	}

	struct {
		Cache next;
		Cache prev;
	} m_cache;
	Tracklist m_tracklist;
	capo::PCM m_current;
	std::size_t m_idx{};
	Mode m_mode = Mode::eStream;
};

class Player {
  public:
	Player(ktl::not_null<capo::Instance*> instance) { ktl::klock(m_shared)->music = capo::Music(instance); }

	bool run(Tracklist tracklist, Playlist::Mode mode) {
		if (!init(std::move(tracklist), mode)) { return false; }
		m_thread = ktl::kthread([this](ktl::kthread::stop_t stop) {
			while (!stop.stop_requested()) {
				if (play_next()) {
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

	bool init(Tracklist tracklist, Playlist::Mode mode) {
		ktl::klock lock(m_shared);
		if (!lock->playlist.load(std::move(tracklist), mode)) { return false; }
		load(lock->music, lock->playlist);
		return true;
	}

	void menu() const {
		ktl::klock lock(m_shared);
		auto const length = capo::utils::Length(lock->music.position());
		std::cout << '\n' << lock->playlist.path();
		if (lock->playlist.mode() == Playlist::Mode::ePreload) { std::cout << " [preloaded]"; }
		std::cout << ktl::kformat(" [{:.2f} gain] [{}]", lock->music.gain(), length) << '\n';
		std::cout << "\n == " << g_stateNames[lock->music.state()] << " ==";
		if (lock->playlist.multi_track()) { std::cout << " [" << lock->playlist.index() + 1 << '/' << lock->playlist.size() << ']'; }
		std::cout << "\n  [t/g] <value>\t: seek to seconds / set gain";
		if (m_state.load() == State::ePlaying) {
			std::cout << "\n  [p/s]\t\t: pause / stop";
		} else {
			std::cout << "\n  [p]\t\t: play";
		}
		if (lock->playlist.multi_track()) { std::cout << "\n  [</>]\t\t: previous / next"; }
		std::cout << "\n  [q]\t\t: quit\n  [?]\t\t: refresh\n: " << std::flush;
	}

	bool input() {
		char ch;
		std::cin >> ch;
		switch (ch) {
		case 't': {
			float time;
			std::cin >> time;
			if (!ktl::klock(m_shared)->music.seek(capo::Time(time))) { std::cerr << "\nseek fail!\n"; }
			break;
		}
		case 's': {
			ktl::klock(m_shared)->music.stop();
			m_state = State::eStopped;
			break;
		}
		case 'p': {
			ktl::klock lock(m_shared);
			if (lock->music.state() == capo::State::ePlaying) {
				lock->music.pause();
			} else {
				lock->music.play();
			}
			m_state = State::ePlaying;
			break;
		}
		case 'g': {
			float gain;
			std::cin >> gain;
			if (!ktl::klock(m_shared)->music.gain(gain)) { std::cerr << "\ngain fail!\n"; }
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

	bool play_next() {
		ktl::klock lock(m_shared);
		auto const stopped = lock->music.state() == capo::State::eStopped;
		auto const last = lock->playlist.is_last_track();
		if (last && stopped) { m_state = State::eStopped; }
		return m_state.load() == State::ePlaying && stopped && !last;
	}

	void play() { ktl::klock(m_shared)->music.play(); }

	void next() {
		ktl::klock lock(m_shared);
		if (lock->playlist.multi_track()) {
			lock->playlist.next();
			advance(lock->music, lock->playlist);
		}
	}

	void prev() {
		ktl::klock lock(m_shared);
		if (lock->playlist.multi_track()) {
			lock->playlist.prev();
			advance(lock->music, lock->playlist);
		}
	}

	void load(capo::Music& out_music, Playlist const& playlist) {
		if (playlist.mode() == Playlist::Mode::ePreload) {
			out_music.preload(playlist.pcm());
		} else {
			out_music.open(playlist.path().c_str());
		}
		auto const& meta = out_music.meta();
		std::cout << ktl::kformat("\n  {}\n\t{:.1f}s Length\n\t{} Channel(s)\n\t{} Sample Rate\n\t{} Size\n", playlist.path(), meta.length().count(),
								  meta.channel_count(meta.format), out_music.sample_rate(), out_music.size());
	}

	void advance(capo::Music& out_music, Playlist const& playlist) {
		out_music.stop();
		load(out_music, playlist);
		if (m_state.load() == State::ePlaying) { out_music.play(); }
	}

	struct Shared {
		Playlist playlist;
		capo::Music music;
	};

	ktl::strict_tmutex<Shared> m_shared;
	ktl::kthread m_thread;
	std::atomic<State> m_state = State::eStopped;
};

std::vector<std::string> build_tracklist(std::string_view path) {
	std::vector<std::string> ret;
	if (auto file = std::ifstream(path.data())) {
		for (std::string line; std::getline(file, line); line.clear()) { ret.push_back(std::move(line)); }
	}
	return ret;
}

std::string_view app_name(std::string_view path) {
	auto it = path.find_last_of('/');
	if (it == std::string_view::npos) { it = path.find_last_of('\\'); }
	if (it != std::string_view::npos) { return path.substr(it + 1); }
	return path;
}
} // namespace

int main(int argc, char const* const argv[]) {
	constexpr std::string_view playlist_name = "capo_playlist.txt";
	int argi{};
	std::string_view const name = app_name(argv[argi++]);
	Tracklist tracklist;
	auto mode = Playlist::Mode::eStream;
	if (argi < argc) {
		if (std::string_view const arg(argv[argi]); arg == "--preload" || arg == "-p") {
			mode = Playlist::Mode::ePreload;
			++argi;
		}
	}
	if (argi == argc) {
		if (tracklist = build_tracklist(playlist_name); tracklist.empty()) {
			std::cerr << "Usage: " << name << " [-p|--preload] [capo_playlist.txt] <file_path0> [file_path1 ...]" << std::endl;
			return fail_code;
		}
	}
	if (argi < argc) {
		if (std::string_view const arg = argv[argi]; arg.ends_with(".txt")) {
			tracklist = build_tracklist(arg);
			++argi;
		}
	}
	for (; argi < argc; ++argi) { tracklist.push_back(argv[argi]); }
	auto instance = capo::Instance::make();
	if (!instance->valid()) {
		std::cerr << "Failed to create instance" << std::endl;
		return fail_code;
	}
	Player player(instance.get());
	if (!player.run(valid_tracks(std::move(tracklist), instance.get()), mode)) { return fail_code; }
}
