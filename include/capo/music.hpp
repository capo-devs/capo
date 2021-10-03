#pragma once
#include <capo/pcm.hpp>
#include <capo/source.hpp>
#include <ktl/not_null.hpp>
#include <memory>

namespace capo {
class Instance;

///
/// \brief Streams music from filesystem / memory
///
/// Requires a pointer to an existing instance to activate
/// Uses a polling thread to swap buffers
///
class Music {
  public:
	Music();
	Music(ktl::not_null<Instance*> instance);
	Music(Music&&) noexcept;
	Music& operator=(Music&&) noexcept;
	~Music();

	///
	/// \brief Check if Instance is valid
	///
	bool valid() const noexcept;
	///
	/// \brief Check if source is ready to play
	///
	bool ready() const;

	///
	/// \brief Open a file at path for streaming
	///
	Outcome open(std::string path);
	///
	/// \brief Preload pcm for streaming
	///
	Outcome preload(PCM pcm);

	bool play();
	bool pause();
	bool stop();

	bool gain(float value);
	float gain() const;
	bool pitch(float value);
	float pitch() const;
	bool loop(bool value);
	bool looping() const;
	Metadata const& meta() const;
	utils::Size size() const;
	utils::Rate sampleRate() const noexcept;
	bool playing() const;
	Time played() const;

  private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	Instance* m_instance{};
};
} // namespace capo
