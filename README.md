# Capo

### Minmalist C++20 audio library

[![Build Status](https://github.com/capo-devs/capo/actions/workflows/ci.yml/badge.svg)](https://github.com/capo-devs/capo/actions/workflows/ci.yml)

#### Features

- Audio clip playback (direct)
- Audio source 3D position
- Music playback (file / in-memory streaming)
- RAII types
- Exception-less implementation
- Error callback (optional)

#### Requirements

- C++20 compiler (and standard library)
- CMake 3.14+ (Ideally 3.20+)

#### Supported Formats

- WAV
- FLAC
- MP3

#### Usage

```cpp
#include <capo/capo.hpp>

void capoTest() {
  auto instance = capo::Instance::make();
  auto pcm = capo::PCM::from_file("audio_clip.wav"); // load/decompress audio file into PCM
  capo::Sound sound = instance->make_sound(*pcm); // make a new Sound instance using above PCM
  capo::Source source = instance->make_source(); // make a new Source instance
  source.play(sound); // bind Sound instance to Source and start playing
  while (source.state() == capo::State::ePlaying) {
    std::this_thread::yield(); // wait until playback complete
  }
  capo::Music music(instance.get()); // construct new music instance
  music.open("music_file.mp3"); // open file in streaming mode
  music.play(); // start playback
  while (music.state() == capo::State::ePlaying) {
    std::this_thread::yield(); // wait until playback complete
  }
}
```

[example_sound](example/example_sound.cpp) and [example_music](example/example_music.cpp) demonstrate basic sound and music usage, [music_player](example/music_player.cpp) demonstrates a more featured multi-track console music player.

#### Dependencies

- [OpenAL Soft](https://github.com/kcat/openal-soft)
- [dr_libs](https://github.com/capo-devs/dr_libs) (forked; [original repo](https://github.com/mackron/dr_libs))

#### Misc

[Original repository](https://github.com/capo-devs/capo)

[LICENCE](LICENSE)
