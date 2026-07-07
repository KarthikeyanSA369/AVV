/**
 * @file audio.cpp
 * @brief SDL2 audio output implementation with FFmpeg resampling.
 */

#include "audio.h"
#include <cstring>
#include <algorithm>
#include <iostream>

extern "C" {
#include <libavutil/mathematics.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/samplefmt.h>
}

namespace myplayer {

/**
 * @brief Construct a new AudioOutput.
 */
AudioOutput::AudioOutput() = default;

/**
 * @brief Destructor. Ensures clean shutdown.
 */
AudioOutput::~AudioOutput() {
    Shutdown();
}

/**
 * @brief Initialize the SDL audio subsystem and open the default playback device.
 */
bool AudioOutput::Initialize() {
    if (initialized_.load()) {
        return true;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL_Init audio failed: " << SDL_GetError() << std::endl;
        return false;
    }

    SDL_AudioSpec desiredSpec{};
    desiredSpec.freq = kTargetSampleRate;
    desiredSpec.format = AUDIO_S16SYS;
    desiredSpec.channels = kTargetChannels;
    desiredSpec.samples = 4096; // Buffer size in samples.
    desiredSpec.callback = AudioCallback;
    desiredSpec.userdata = this;

    deviceId_ = SDL_OpenAudioDevice(
        nullptr,           // default device
        0,                 // playback
        &desiredSpec,
        &obtainedSpec_,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE
    );

    if (deviceId_ == 0) {
        std::cerr << "SDL_OpenAudioDevice failed: " << SDL_GetError() << std::endl;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return false;
    }

    initialized_.store(true);
    return true;
}

/**
 * @brief Close the audio device and clean up SDL audio resources.
 */
void AudioOutput::Shutdown() {
    if (!initialized_.load()) {
        return;
    }

    Pause();
    SDL_CloseAudioDevice(deviceId_);
    deviceId_ = 0;

    if (swrCtx_) {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
    }

    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        audioBuffer_.clear();
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized_.store(false);
}

/**
 * @brief Start audio playback (unpause the SDL audio device).
 */
void AudioOutput::Play() {
    if (initialized_.load() && !playing_.load()) {
        SDL_PauseAudioDevice(deviceId_, 0);
        playing_.store(true);
    }
}

/**
 * @brief Pause audio playback.
 */
void AudioOutput::Pause() {
    if (initialized_.load() && playing_.load()) {
        SDL_PauseAudioDevice(deviceId_, 1);
        playing_.store(false);
    }
}

/**
 * @brief Set the master volume level.
 */
void AudioOutput::SetVolume(float volume) {
    volume_.store(std::clamp(volume, 0.0f, 1.0f));
}

/**
 * @brief SDL audio callback. Called on a dedicated audio thread by SDL.
 */
void SDLCALL AudioOutput::AudioCallback(void* userdata, Uint8* stream, int len) {
    auto* audio = static_cast<AudioOutput*>(userdata);
    if (!audio || !audio->initialized_.load()) {
        std::memset(stream, 0, len);
        return;
    }
    audio->FillAudioBuffer(stream, len);
}

/**
 * @brief Fill the SDL audio buffer with resampled PCM data.
 */
void AudioOutput::FillAudioBuffer(Uint8* stream, int len) {
    // Silence the buffer first.
    std::memset(stream, 0, len);

    std::lock_guard<std::mutex> lock(bufferMutex_);

    // Pull frames from decoder if buffer is low.
    while (decoder_ && static_cast<int>(audioBuffer_.size()) < len * 2) {
        AudioFrame frame;
        if (!decoder_->GetAudioQueue().Pop(frame)) {
            break;
        }

        // Update audio clock.
        audioClock_.store(frame.pts);

        // Initialize resampler if needed.
        if (!swrCtx_ || frame.sampleRate != currentSrcRate_ ||
            frame.channels != currentSrcChannels_ || frame.format != currentSrcFmt_) {
            if (!InitResampler(frame.sampleRate, frame.channels, frame.format)) {
                continue;
            }
        }

        int bytesProduced = ResampleFrame(frame.frame.get());
        if (bytesProduced <= 0) {
            continue;
        }
    }

    // Copy from our buffer to SDL's buffer.
    int bytesToCopy = std::min(len, static_cast<int>(audioBuffer_.size()));
    if (bytesToCopy > 0) {
        std::memcpy(stream, audioBuffer_.data(), bytesToCopy);
        audioBuffer_.erase(audioBuffer_.begin(), audioBuffer_.begin() + bytesToCopy);
    }

    // Apply volume scaling.
    float vol = volume_.load();
    if (vol < 1.0f) {
        auto* samples = reinterpret_cast<int16_t*>(stream);
        int sampleCount = len / sizeof(int16_t);
        for (int i = 0; i < sampleCount; ++i) {
            samples[i] = static_cast<int16_t>(samples[i] * vol);
        }
    }
}

/**
 * @brief Resample a single AVFrame and append to the internal audio buffer.
 */
int AudioOutput::ResampleFrame(AVFrame* frame) {
    if (!swrCtx_ || !frame) {
        return 0;
    }

    // Calculate output sample count.
    int64_t delay = swr_get_delay(swrCtx_, frame->sample_rate);
    int64_t outSamples = av_rescale_rnd(
        delay + frame->nb_samples,
        kTargetSampleRate,
        frame->sample_rate,
        AV_ROUND_UP
    );

    // Allocate temporary buffer for resampled data.
    int bytesPerSample = av_get_bytes_per_sample(kTargetFormat) * kTargetChannels;
    std::vector<uint8_t> outBuffer(static_cast<size_t>(outSamples) * bytesPerSample);
    uint8_t* outData[1] = { outBuffer.data() };
    int outLinesize[1] = { static_cast<int>(outBuffer.size()) };

    int converted = swr_convert(
        swrCtx_,
        outData, static_cast<int>(outSamples),
        const_cast<const uint8_t**>(frame->data), frame->nb_samples
    );

    if (converted < 0) {
        return 0;
    }

    int bytesProduced = converted * bytesPerSample;
    audioBuffer_.insert(audioBuffer_.end(), outBuffer.begin(), outBuffer.begin() + bytesProduced);
    return bytesProduced;
}

/**
 * @brief Initialize the SwrContext for converting from source to target format.
 */
bool AudioOutput::InitResampler(int srcRate, int srcChannels, AVSampleFormat srcFmt)
{
    if (swrCtx_)
    {
        swr_free(&swrCtx_);
        swrCtx_ = nullptr;
    }

    AVChannelLayout srcLayout{};
    AVChannelLayout dstLayout{};

    // Create layouts without using AV_CHANNEL_LAYOUT_STEREO
    av_channel_layout_default(&srcLayout, srcChannels);
    av_channel_layout_default(&dstLayout, kTargetChannels);

    int ret = swr_alloc_set_opts2(
        &swrCtx_,
        &dstLayout,
        kTargetFormat,
        kTargetSampleRate,
        &srcLayout,
        srcFmt,
        srcRate,
        0,
        nullptr
    );

    av_channel_layout_uninit(&srcLayout);
    av_channel_layout_uninit(&dstLayout);

    if (ret < 0 || !swrCtx_)
    {
        std::cerr << "Failed to allocate SwrContext" << std::endl;
        swrCtx_ = nullptr;
        return false;
    }

    ret = swr_init(swrCtx_);

    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));

        std::cerr << "swr_init failed: "
                  << errbuf
                  << std::endl;

        swr_free(&swrCtx_);
        return false;
    }

    currentSrcRate_ = srcRate;
    currentSrcChannels_ = srcChannels;
    currentSrcFmt_ = srcFmt;

    return true;
}
