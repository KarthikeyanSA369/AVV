/**
 * @file player.h
 * @brief High-level media player controller.
 *
 * Orchestrates the Decoder, Renderer, AudioOutput, and SeekBar into a
 * cohesive playback experience with state management and event handling.
 */

#pragma once

#include "decoder.h"
#include "renderer.h"
#include "audio.h"
#include "seekbar.h"
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

namespace myplayer {

/**
 * @brief Playback states.
 */
enum class PlayerState {
    Idle,       ///< No media loaded.
    Loading,    ///< Media is being opened.
    Playing,    ///< Actively playing.
    Paused,     ///< Paused.
    Stopped,    ///< Stopped after playback or error.
    Seeking     ///< Currently seeking.
};

/**
 * @brief High-level media player that coordinates all subsystems.
 */
class Player {
public:
    /**
     * @brief Construct a new Player.
     */
    Player();

    /**
     * @brief Destructor. Ensures clean shutdown.
     */
    ~Player();

    // Non-copyable, non-movable.
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(Player&&) = delete;

    /**
     * @brief Initialize all subsystems (SDL video/audio, window, etc.).
     * @param windowTitle Title for the application window.
     * @param windowWidth Initial window width.
     * @param windowHeight Initial window height.
     * @return true on success, false on failure.
     */
    bool Initialize(const std::string& windowTitle, int windowWidth, int windowHeight);

    /**
     * @brief Shutdown all subsystems and release resources.
     */
    void Shutdown();

    /**
     * @brief Load and prepare a media file for playback.
     * @param filepath Path to the media file.
     * @return true on success, false on failure.
     */
    bool LoadMedia(const std::string& filepath);

    /**
     * @brief Start or resume playback.
     */
    void Play();

    /**
     * @brief Pause playback.
     */
    void Pause();

    /**
     * @brief Stop playback and reset position.
     */
    void Stop();

    /**
     * @brief Seek to a specific time.
     * @param targetTime Target time in seconds.
     */
    void Seek(double targetTime);

    /**
     * @brief Set the master volume.
     * @param volume Volume from 0.0 to 1.0.
     */
    void SetVolume(float volume);

    /**
     * @brief Get the current volume.
     */
    float GetVolume() const;

    /**
     * @brief Get the current playback state.
     */
    PlayerState GetState() const { return state_.load(); }

    /**
     * @brief Get the current playback position in seconds.
     */
    double GetCurrentTime() const { return currentTime_.load(); }

    /**
     * @brief Get the total media duration in seconds.
     */
    double GetDuration() const { return duration_.load(); }

    /**
     * @brief Run the main event loop. Blocks until the window is closed.
     * @return Exit code (0 for normal exit).
     */
    int RunEventLoop();

    /**
     * @brief Toggle fullscreen mode.
     */
    void ToggleFullscreen();

    /**
     * @brief Check if the player is initialized.
     */
    bool IsInitialized() const { return initialized_.load(); }

private:
    /**
     * @brief Video rendering thread: pulls frames and renders them.
     */
    void VideoRenderThread();

    /**
     * @brief Update the seek bar with current playback position.
     */
    void UpdateSeekBar();

    /**
     * @brief Handle SDL events (keyboard, mouse, window, quit).
     * @param event The SDL event to process.
     * @return true if the application should quit.
     */
    bool ProcessEvent(const SDL_Event& event);

    std::unique_ptr<Decoder> decoder_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<AudioOutput> audioOutput_;
    std::unique_ptr<SeekBar> seekBar_;

    std::atomic<PlayerState> state_{PlayerState::Idle};
    std::atomic<bool> initialized_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> quitRequested_{false};
    std::atomic<double> currentTime_{0.0};
    std::atomic<double> duration_{0.0};

    // Threading.
    std::thread videoRenderThread_;
    mutable std::mutex stateMutex_;

    // Window state.
    bool fullscreen_{false};
    int windowWidth_{1280};
    int windowHeight_{720};
};

} // namespace myplayer