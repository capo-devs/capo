#include <capo/capo.hpp>
#include <ktl/kthread.hpp>
#include <ktl/str_format.hpp>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include <ktl/async.hpp>
#include <ktl/future.hpp>
#include <ktl/tmutex.hpp>
#include <atomic>

namespace {
static constexpr int fail_code = 2;

constexpr capo::utils::EnumArray<capo::State, std::string_view> g_stateNames = {
	"UNKNOWN", "IDLE", "PLAYING", "PAUSED", "STOPPED",
};

constexpr bool isLast(std::size_t index, std::size_t total) { return index + 1 == total; }

using Tracklist = std::vector<std::string>;

Tracklist validTracks(Tracklist tracklist, ktl::not_null<capo::Instance*> instance) {
	Tracklist ret;
	ret.reserve(tracklist.size());
	capo::Music music(instance);
	for (auto& track : tracklist) {
		if (!music.open(track)) {
			std::cerr << "Failed to open " << track << std::endl;
			continue;
		}
		ret.push_back(std::move(track));
	}
	return ret;
}

class Playlist {
  public:
	bool load(Tracklist tracklist) {
		if (tracklist.empty()) {
			std::cerr << "Failed to load any valid tracks";
			return false;
		}
		m_tracklist = std::move(tracklist);
		m_current = load(m_tracklist[m_idx]);
		loadCache();
		return true;
	}

	std::size_t index() const noexcept { return m_idx; }
	std::size_t size() const noexcept { return m_tracklist.size(); }
	bool multiTrack() const noexcept { return size() > 1; }
	bool isLastTrack() const noexcept { return isLast(index(), size()); }
	std::string_view path() const noexcept { return m_tracklist[index()]; }
	capo::PCM const& current() const noexcept { return m_current; }

	capo::PCM const& next() {
		if (multiTrack()) {
			m_idx = nextIdx();
			m_current = std::move(m_cache.next)();
			loadCache();
		}
		return current();
	}

	capo::PCM const& prev() {
		if (multiTrack()) {
			m_idx = prevIdx();
			auto& cache = m_cache.prev.fence.valid() ? m_cache.prev : m_cache.next;
			m_current = std::move(cache)();
			loadCache();
		}
		return current();
	}

  private:
	struct Cache {
		ktl::future<void> fence;
		capo::PCM pcm;

		capo::PCM operator()() && {
			fence.wait();
			return std::move(pcm);
		}
	};

	static capo::PCM load(std::string const& path) { return *capo::PCM::fromFile(path); }

	std::size_t nextIdx() const noexcept { return (m_idx + 1) % m_tracklist.size(); }
	std::size_t prevIdx() const noexcept { return (m_idx + m_tracklist.size() - 1) % m_tracklist.size(); }

	void loadCache() {
		if (multiTrack()) {
			if (m_cache.next.fence.busy()) { m_cache.next.fence.wait(); }
			m_cache.next.fence = m_async([this]() { m_cache.next.pcm = load(m_tracklist[nextIdx()]); });
		}
		if (m_tracklist.size() > 2) {
			if (m_cache.prev.fence.busy()) { m_cache.prev.fence.wait(); }
			m_cache.prev.fence = m_async([this]() { m_cache.prev.pcm = load(m_tracklist[prevIdx()]); });
		}
	}

	struct {
		Cache next;
		Cache prev;
	} m_cache;
	Tracklist m_tracklist;
	capo::PCM m_current;
	std::size_t m_idx{};
	ktl::async m_async; // block destruction of any members until this is destroyed
};

class Player {
  public:
	Player(ktl::not_null<capo::Instance*> instance) { ktl::tlock(m_shared)->music = capo::Music(instance); }

	bool init(Tracklist tracklist) {
		ktl::tlock lock(m_shared);
		if (!lock->playlist.load(std::move(tracklist))) { return false; }
		load(lock->music, lock->playlist);
		return true;
	}

	bool run(Tracklist tracklist) {
		if (!init(std::move(tracklist))) { return false; }
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

	void menu() const {
		ktl::tlock lock(m_shared);
		auto const length = capo::utils::Length(lock->music.position());
		std::cout << '\n' << lock->playlist.path() << " [" << std::fixed << std::setprecision(2) << lock->music.gain() << " gain] [" << length << "]";
		std::cout << "\n == " << g_stateNames[lock->music.state()] << " ==";
		if (lock->playlist.multiTrack()) { std::cout << " [" << lock->playlist.index() + 1 << '/' << lock->playlist.size() << ']'; }
		std::cout << "\n  [t/g] <value>\t: seek to seconds / set gain";
		if (m_state.load() == State::ePlaying) {
			std::cout << "\n  [p/s]\t\t: pause / stop";
		} else {
			std::cout << "\n  [p]\t\t: play";
		}
		if (lock->playlist.multiTrack()) { std::cout << "\n  [</>]\t\t: previous / next"; }
		std::cout << "\n  [q]\t\t: quit\n  [?]\t\t: refresh\n: " << std::flush;
	}

	bool input() {
		char ch;
		std::cin >> ch;
		switch (ch) {
		case 't': {
			float time;
			std::cin >> time;
			if (!ktl::tlock(m_shared)->music.seek(capo::Time(time))) { std::cerr << "\nseek fail!\n"; }
			break;
		}
		case 's': {
			ktl::tlock(m_shared)->music.stop();
			m_state = State::eStopped;
			break;
		}
		case 'p': {
			ktl::tlock lock(m_shared);
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
			if (!ktl::tlock(m_shared)->music.gain(gain)) { std::cerr << "\ngain fail!\n"; }
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
		ktl::tlock lock(m_shared);
		auto const stopped = lock->music.state() == capo::State::eStopped;
		auto const last = lock->playlist.isLastTrack();
		if (last && stopped) { m_state = State::eStopped; }
		return m_state.load() == State::ePlaying && stopped && !last;
	}

	void play() { ktl::tlock(m_shared)->music.play(); }

	void next() {
		ktl::tlock lock(m_shared);
		if (lock->playlist.multiTrack()) {
			lock->playlist.next();
			advance(lock->music, lock->playlist);
		}
	}

	void prev() {
		ktl::tlock lock(m_shared);
		if (lock->playlist.multiTrack()) {
			lock->playlist.prev();
			advance(lock->music, lock->playlist);
		}
	}

	void load(capo::Music& out_music, Playlist const& playlist) {
		out_music.preload(playlist.current());
		auto const& meta = out_music.meta();
		std::cout << ktl::format("\n  {}\n\t{.1f}s Length\n\t{} Channel(s)\n\t{} Sample Rate\n\t{} Size\n", playlist.path(), meta.length().count(),
								 meta.channelCount(meta.format), out_music.sampleRate(), out_music.size());
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

std::vector<std::string> buildTracklist(std::string_view path) {
	std::vector<std::string> ret;
	if (auto file = std::ifstream(path.data())) {
		for (std::string line; std::getline(file, line); line.clear()) { ret.push_back(std::move(line)); }
	}
	return ret;
}

std::string_view appName(std::string_view path) {
	auto it = path.find_last_of('/');
	if (it == std::string_view::npos) { it = path.find_last_of('\\'); }
	if (it != std::string_view::npos) { return path.substr(it + 1); }
	return path;
}
} // namespace

int main(int argc, char const* const argv[]) {
	constexpr std::string_view playlistName = "capo_playlist.txt";
	std::string_view const name = appName(argv[0]);
	Tracklist tracklist;
	if (argc < 2) {
		if (tracklist = buildTracklist(playlistName); tracklist.empty()) {
			std::cerr << "Usage: " << name << " [capo_playlist.txt] <file_path0> [file_path1 ...]" << std::endl;
			return fail_code;
		}
	}
	if (argc > 1) {
		std::string_view const argv1 = argv[1];
		if (argv1.ends_with(".txt")) { tracklist = buildTracklist(argv1); }
	}
	if (!tracklist.empty()) {
		for (int i = 2; i < argc; ++i) { tracklist.push_back(argv[i]); }
	} else {
		auto const span = std::span(argv + 1, std::size_t(argc - 1));
		tracklist = {span.begin(), span.end()};
	}
	capo::Instance instance;
	if (!instance.valid()) {
		std::cerr << "Failed to create instance" << std::endl;
		return fail_code;
	}
	Player player(&instance);
	if (!player.run(validTracks(std::move(tracklist), &instance))) { return fail_code; }
}
