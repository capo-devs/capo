#pragma once
#include <capo/pcm.hpp>
#include <capo/source.hpp>
#include <ktl/kunique_ptr.hpp>
#include <ktl/not_null.hpp>

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
	Result<void> open(char const* path);
	///
	/// \brief Preload pcm for streaming
	///
	Result<void> preload(PCM pcm);

	bool play();
	bool pause();
	bool stop();

	bool gain(float value);
	float gain() const;
	bool pitch(float value);
	float pitch() const;
	bool loop(bool value);
	bool looping() const;
	Result<void> seek(Time stamp);
	Time position() const;

	Metadata const& meta() const;
	utils::Size size() const;
	utils::Rate sample_rate() const;

	State state() const;

  private:
	struct Impl;
	ktl::kunique_ptr<Impl> m_impl{};
	Instance* m_instance{};
};
} // namespace capo
