/**
 * @file player.cpp
 * @brief High-level player controller implementation.
 */

#include "player.h"
#include <iostream>
#include <chrono>

namespace myplayer {

/**
 * @brief Construct a new Player with default-initialized subsystems.
 */
Player::Player()
    : decoder_(std::make_unique<Decoder>())
    , renderer_(std::make_unique<Renderer>())
    , audioOutput_(std::make_unique<AudioOutput>())
    , seekBar_(std::make_unique<SeekBar>())
{
}

/**
 * @brief Destructor. Ensures clean shutdown of all subsystems.
 */
Player::~Player() {
    Shutdown();
}

/**
 * @brief Initialize SDL subsystems, create window, renderer, and audio device.
 */
bool Player::Initialize(const std::string& windowTitle, int windowWidth, int windowHeight) {
    if (initialized_.load()) {
        return true;
    }

    windowWidth_ = windowWidth;
    windowHeight_ = windowHeight;

    // Initialize video renderer.
    if (!renderer_->Initialize(windowTitle, windowWidth, windowHeight)) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        return false;
    }

    // Initialize audio output.
    if (!audioOutput_->Initialize()) {
        std::cerr << "Failed to initialize audio output" << std::endl;
        renderer_->Shutdown();
        return false;
    }

    // Set up seek bar callback.
    seekBar_->SetSeekCallback([this](double targetTime) {
        this->Seek(targetTime);
    });

    initialized_.store(true);
    state_.store(PlayerState::Idle);
    return true;
}

/**
 * @brief Shutdown all subsystems and release resources.
 */
void Player::Shutdown() {
    if (!initialized_.load()) {
        return;
    }

    Stop();
    running_.store(false);
    quitRequested_.store(true);

    if (videoRenderThread_.joinable()) {
        videoRenderThread_.join();
    }

    audioOutput_->Shutdown();
    renderer_->Shutdown();
    decoder_->Close();

    initialized_.store(false);
    state_.store(PlayerState::Idle);
}

/**
 * @brief Load a media file, open decoder, and prepare for playback.
 */
bool Player::LoadMedia(const std::string& filepath) {
    if (!initialized_.load()) {
        return false;
    }

    Stop();
    state_.store(PlayerState::Loading);

    if (!decoder_->Open(filepath)) {
        state_.store(PlayerState::Stopped);
        std::cerr << "Failed to open media: " << filepath << std::endl;
        return false;
    }

    duration_.store(decoder_->GetDuration());
    seekBar_->SetDuration(decoder_->GetDuration());
    currentTime_.store(0.0);

    // Link decoder to audio output.
    audioOutput_->SetDecoder(decoder_.get());

    state_.store(PlayerState::Paused);
    return true;
}

/**
 * @brief Start playback from current position.
 */
void Player::Play() {
    if (state_.load() == PlayerState::Idle || state_.load() == PlayerState::Loading) {
        return;
    }

    if (state_.load() == PlayerState::Stopped) {
        // Restart from beginning.
        decoder_->Seek(0.0);
        currentTime_.store(0.0);
    }

    decoder_->Start();
    audioOutput_->Play();

    if (!videoRenderThread_.joinable()) {
        running_.store(true);
        videoRenderThread_ = std::thread(&Player::VideoRenderThread, this);
    }

    state_.store(PlayerState::Playing);
}

/**
 * @brief Pause playback.
 */
void Player::Pause() {
    if (state_.load() != PlayerState::Playing) {
        return;
    }

    audioOutput_->Pause();
    state_.store(PlayerState::Paused);
}

/**
 * @brief Stop playback and reset position.
 */
void Player::Stop() {
    audioOutput_->Pause();
    decoder_->Stop();
    running_.store(false);

    if (videoRenderThread_.joinable()) {
        videoRenderThread_.join();
    }

    currentTime_.store(0.0);
    seekBar_->SetCurrentTime(0.0);
    state_.store(PlayerState::Stopped);
}

/**
 * @brief Seek to a target time in the media.
 */
void Player::Seek(double targetTime) {
    if (state_.load() == PlayerState::Idle || state_.load() == PlayerState::Loading) {
        return;
    }

    double clampedTime = std::max(0.0, std::min(targetTime, duration_.load()));
    currentTime_.store(clampedTime);
    seekBar_->SetCurrentTime(clampedTime);

    decoder_->Seek(clampedTime);
}

/**
 * @brief Set the master volume level.
 */
void Player::SetVolume(float volume) {
    audioOutput_->SetVolume(volume);
}

/**
 * @brief Get the current master volume.
 */
float Player::GetVolume() const {
    return audioOutput_->GetVolume();
}

/**
 * @brief Main event loop: processes SDL events and coordinates rendering.
 */
int Player::RunEventLoop() {
    if (!initialized_.load()) {
        return -1;
    }

    SDL_Event event;
    while (!quitRequested_.load()) {
        while (SDL_PollEvent(&event)) {
            if (ProcessEvent(event)) {
                return 0;
            }
        }

        // Update seek bar position from audio clock when playing.
        if (state_.load() == PlayerState::Playing) {
            double audioClock = audioOutput_->GetAudioClock();
            if (audioClock > 0) {
                currentTime_.store(audioClock);
                seekBar_->SetCurrentTime(audioClock);
            }
        }

        // Render seek bar overlay.
        if (renderer_->IsInitialized()) {
            seekBar_->Render(
                renderer_->GetWindow() ? SDL_GetRenderer(renderer_->GetWindow()) : nullptr,
                windowWidth_, windowHeight_
            );
        }

        // Cap event loop to ~60 FPS to avoid burning CPU.
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
}

/**
 * @brief Toggle between windowed and fullscreen modes.
 */
void Player::ToggleFullscreen() {
    if (!renderer_->GetWindow()) {
        return;
    }

    fullscreen_ = !fullscreen_;
    Uint32 flags = fullscreen_ ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0;
    SDL_SetWindowFullscreen(renderer_->GetWindow(), flags);
}

/**
 * @brief Video rendering thread: consumes decoded frames and renders them.
 */
void Player::VideoRenderThread() {
    while (running_.load()) {
        if (state_.load() != PlayerState::Playing) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        VideoFrame frame;
        if (decoder_->GetVideoQueue().Pop(frame)) {
            renderer_->RenderFrame(frame);

            // Simple frame timing: sleep to match frame duration.
            if (frame.duration > 0) {
                auto sleepMs = static_cast<int>(frame.duration * 1000);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
            }
        } else if (decoder_->GetVideoQueue().IsShutdown()) {
            // Queue shut down, video ended.
            break;
        }
    }
}

/**
 * @brief Process a single SDL event.
 * @return true if the application should quit.
 */
bool Player::ProcessEvent(const SDL_Event& event) {
    switch (event.type) {
        case SDL_QUIT:
            quitRequested_.store(true);
            return true;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
                case SDLK_SPACE:
                    if (state_.load() == PlayerState::Playing) {
                        Pause();
                    } else {
                        Play();
                    }
                    break;
                case SDLK_s:
                    Stop();
                    break;
                case SDLK_f:
                    ToggleFullscreen();
                    break;
                case SDLK_ESCAPE:
                    if (fullscreen_) {
                        ToggleFullscreen();
                    } else {
                        quitRequested_.store(true);
                        return true;
                    }
                    break;
                case SDLK_UP:
                    SetVolume(std::min(1.0f, GetVolume() + 0.1f));
                    break;
                case SDLK_DOWN:
                    SetVolume(std::max(0.0f, GetVolume() - 0.1f));
                    break;
                case SDLK_LEFT:
                    Seek(GetCurrentTime() - 10.0);
                    break;
                case SDLK_RIGHT:
                    Seek(GetCurrentTime() + 10.0);
                    break;
                default:
                    break;
            }
            break;

        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                windowWidth_ = event.window.data1;
                windowHeight_ = event.window.data2;
                renderer_->Resize(windowWidth_, windowHeight_);
            }
            break;

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEMOTION:
            if (seekBar_->HandleMouseEvent(event, windowWidth_, windowHeight_)) {
                return false; // Event consumed by seek bar.
            }
            break;

        default:
            break;
    }

    return false;
}

} // namespace myplayer