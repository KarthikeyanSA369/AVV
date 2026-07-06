/**
 * @file audio.h
 * @brief SDL2-based audio output with FFmpeg resampling.
 *
 * Pulls decoded audio frames from the decoder, resamples them to the
 * device format, and feeds them to the SDL audio callback.
 */

#pragma once

#include "decoder.h"
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <thread>

#include <SDL.h>

extern "C" {
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
}

namespace myplayer {

/**
 * @brief Target audio output format.
 */
constexpr int kTargetSampleRate = 48000;
constexpr int kTargetChannels = 2;
constexpr AVSampleFormat kTargetFormat = AV_SAMPLE_FMT_S16;

/**
 * @brief SDL2 audio output handler with FFmpeg resampling.
 */
class AudioOutput {
public:
    /**
     * @brief Construct a new AudioOutput.
     */
    AudioOutput();

    /**
     * @brief Destructor. Ensures clean shutdown.
     */
    ~AudioOutput();

    // Non-copyable, non-movable.
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;
    AudioOutput(AudioOutput&&) = delete;
    AudioOutput& operator=(AudioOutput&&) = delete;

    /**
     * @brief Initialize the SDL audio device.
     * @return true on success, false on failure.
     */
    bool Initialize();

    /**
     * @brief Close the audio device and free resources.
     */
    void Shutdown();

    /**
     * @brief Start audio playback.
     */
    void Play();

    /**
     * @brief Pause audio playback.
     */
    void Pause();

    /**
     * @brief Set the decoder to pull audio frames from.
     * @param decoder Pointer to the active decoder.
     */
    void SetDecoder(Decoder* decoder) { decoder_ = decoder; }

    /**
     * @brief Set the master volume (0.0 to 1.0).
     * @param volume Volume level.
     */
    void SetVolume(float volume);

    /**
     * @brief Get the current master volume.
     */
    float GetVolume() const { return volume_.load(); }

    /**
     * @brief Get the current audio clock time in seconds.
     */
    double GetAudioClock() const { return audioClock_.load(); }

    /**
     * @brief Check if the audio device is initialized.
     */
    bool IsInitialized() const { return initialized_.load(); }

    /**
     * @brief Check if audio is currently playing.
     */
    bool IsPlaying() const { return playing_.load(); }

private:
    /**
     * @brief SDL audio callback. Called by SDL on the audio thread.
     * @param userdata Pointer to this AudioOutput instance.
     * @param stream Output buffer to fill.
     * @param len Number of bytes to write.
     */
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len);

    /**
     * @brief Fill the audio buffer with resampled data.
     * @param stream Output buffer.
     * @param len Number of bytes requested.
     */
    void FillAudioBuffer(Uint8* stream, int len);

    /**
     * @brief Resample an AVFrame to the target format and append to the buffer.
     * @param frame The source audio frame.
     * @return Number of bytes produced.
     */
    int ResampleFrame(AVFrame* frame);

    /**
     * @brief Initialize or reinitialize the SwrContext for the given source format.
     * @param srcRate Source sample rate.
     * @param srcChannels Source channel count.
     * @param srcFmt Source sample format.
     * @return true on success.
     */
    bool InitResampler(int srcRate, int srcChannels, AVSampleFormat srcFmt);

    SDL_AudioDeviceID deviceId_{0};
    SDL_AudioSpec obtainedSpec_{};

    SwrContext* swrCtx_{nullptr};
    Decoder* decoder_{nullptr};

    // Resampled audio buffer (thread-safe via mutex).
    std::vector<uint8_t> audioBuffer_;
    std::mutex bufferMutex_;

    std::atomic<bool> initialized_{false};
    std::atomic<bool> playing_{false};
    std::atomic<float> volume_{1.0f};
    std::atomic<double> audioClock_{0.0};

    // Current resampler source parameters.
    int currentSrcRate_{0};
    int currentSrcChannels_{0};
    AVSampleFormat currentSrcFmt_{AV_SAMPLE_FMT_NONE};
};

} // namespace myplayer