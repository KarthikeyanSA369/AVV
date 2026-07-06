/**
 * @file decoder.h
 * @brief FFmpeg-based audio/video decoder with frame queuing.
 *
 * Supports: MP4, MKV, AVI, MOV, MP3, WAV, FLAC, AAC, HEVC, H264, AV1.
 */

#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <thread>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace myplayer {

/**
 * @brief Maximum number of decoded frames to queue per stream.
 */
constexpr size_t kMaxFrameQueueSize = 128;

/**
 * @brief Structure holding a decoded video frame with its timing metadata.
 */
struct VideoFrame {
    std::unique_ptr<AVFrame, decltype(&av_frame_unref)> frame{nullptr, av_frame_unref};
    double pts{};               ///< Presentation timestamp in seconds.
    double duration{};          ///< Frame duration in seconds.
    int width{};                ///< Frame width in pixels.
    int height{};               ///< Frame height in pixels.
    AVPixelFormat format{};     ///< Pixel format of the frame.
};

/**
 * @brief Structure holding decoded audio samples with timing metadata.
 */
struct AudioFrame {
    std::unique_ptr<AVFrame, decltype(&av_frame_unref)> frame{nullptr, av_frame_unref};
    double pts{};               ///< Presentation timestamp in seconds.
    int sampleRate{};           ///< Sample rate in Hz.
    int channels{};             ///< Number of audio channels.
    AVSampleFormat format{};    ///< Sample format.
};

/**
 * @brief Thread-safe queue for decoded frames.
 * @tparam T Frame type (VideoFrame or AudioFrame).
 */
template <typename T>
class FrameQueue {
public:
    /**
     * @brief Push a frame into the queue. Blocks if queue is full.
     * @param frame The frame to enqueue.
     * @return true if successful, false if queue is shutting down.
     */
    bool Push(T frame);

    /**
     * @brief Pop a frame from the queue. Blocks if queue is empty.
     * @param[out] frame The dequeued frame.
     * @return true if successful, false if queue is shutting down.
     */
    bool Pop(T& frame);

    /**
     * @brief Get current queue size.
     */
    size_t Size() const;

    /**
     * @brief Clear all frames from the queue.
     */
    void Clear();

    /**
     * @brief Signal the queue to shut down, unblocking all waiters.
     */
    void Shutdown();

    /**
     * @brief Check if the queue has been shut down.
     */
    bool IsShutdown() const;

private:
    mutable std::mutex mutex_;
    std::condition_variable notFull_;
    std::condition_variable notEmpty_;
    std::queue<T> queue_;
    std::atomic<bool> shutdown_{false};
};

/**
 * @brief Media decoder using FFmpeg. Decodes audio and video streams
 *        into frame queues for consumption by renderer and audio output.
 */
class Decoder {
public:
    /**
     * @brief Construct a new Decoder.
     */
    Decoder();

    /**
     * @brief Destructor. Cleans up all FFmpeg resources.
     */
    ~Decoder();

    // Non-copyable, non-movable (RAII with raw FFmpeg pointers).
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;
    Decoder& operator=(Decoder&&) = delete;

    /**
     * @brief Open a media file and prepare streams for decoding.
     * @param filepath Path to the media file.
     * @return true on success, false on failure.
     */
    bool Open(const std::string& filepath);

    /**
     * @brief Close the current media file and free all resources.
     */
    void Close();

    /**
     * @brief Start the decoding threads.
     */
    void Start();

    /**
     * @brief Stop all decoding threads gracefully.
     */
    void Stop();

    /**
     * @brief Seek to a specific timestamp in the media.
     * @param targetTime Target time in seconds.
     * @return true on success, false on failure.
     */
    bool Seek(double targetTime);

    /**
     * @brief Get the video frame queue for consumption.
     */
    FrameQueue<VideoFrame>& GetVideoQueue() { return videoQueue_; }

    /**
     * @brief Get the audio frame queue for consumption.
     */
    FrameQueue<AudioFrame>& GetAudioQueue() { return audioQueue_; }

    /**
     * @brief Get total media duration in seconds.
     */
    double GetDuration() const { return duration_; }

    /**
     * @brief Check if the media has a video stream.
     */
    bool HasVideo() const { return videoStreamIndex_ >= 0; }

    /**
     * @brief Check if the media has an audio stream.
     */
    bool HasAudio() const { return audioStreamIndex_ >= 0; }

    /**
     * @brief Get the original video width.
     */
    int GetVideoWidth() const { return videoWidth_; }

    /**
     * @brief Get the original video height.
     */
    int GetVideoHeight() const { return videoHeight_; }

    /**
     * @brief Get the audio sample rate.
     */
    int GetAudioSampleRate() const { return audioSampleRate_; }

    /**
     * @brief Get the number of audio channels.
     */
    int GetAudioChannels() const { return audioChannels_; }

    /**
     * @brief Get the audio sample format.
     */
    AVSampleFormat GetAudioSampleFormat() const { return audioSampleFormat_; }

private:
    /**
     * @brief Main demuxing thread: reads packets and dispatches to decoders.
     */
    void DemuxThread();

    /**
     * @brief Video decoding thread: decodes video packets into frames.
     */
    void VideoDecodeThread();

    /**
     * @brief Audio decoding thread: decodes audio packets into frames.
     */
    void AudioDecodeThread();

    /**
     * @brief Decode a single packet for the given codec context and stream index.
     * @param codecCtx The codec context.
     * @param packet The packet to decode.
     * @param streamIndex The stream index.
     * @return 0 on success, negative AVERROR on failure.
     */
    int DecodePacket(AVCodecContext* codecCtx, AVPacket* packet, int streamIndex);

    /**
     * @brief Flush remaining frames from a codec after seeking or EOF.
     * @param codecCtx The codec context to flush.
     * @param isVideo true if flushing video, false for audio.
     */
    void FlushCodec(AVCodecContext* codecCtx, bool isVideo);

    // FFmpeg context (RAII via manual cleanup in dtor).
    AVFormatContext* fmtCtx_{nullptr};
    AVCodecContext* videoCodecCtx_{nullptr};
    AVCodecContext* audioCodecCtx_{nullptr};

    // Stream indices.
    int videoStreamIndex_{-1};
    int audioStreamIndex_{-1};

    // Stream metadata.
    double duration_{0.0};
    int videoWidth_{0};
    int videoHeight_{0};
    int audioSampleRate_{0};
    int audioChannels_{0};
    AVSampleFormat audioSampleFormat_{AV_SAMPLE_FMT_NONE};

    // Frame queues.
    FrameQueue<VideoFrame> videoQueue_;
    FrameQueue<AudioFrame> audioQueue_;

    // Threading.
    std::atomic<bool> running_{false};
    std::thread demuxThread_;
    std::thread videoDecodeThread_;
    std::thread audioDecodeThread_;

    // Synchronization.
    std::mutex seekMutex_;
    std::atomic<double> seekTarget_{-1.0};  ///< -1.0 means no pending seek.
    std::atomic<bool> seekRequested_{false};

    // Packet queues for decoder threads.
    std::queue<AVPacket*> videoPacketQueue_;
    std::queue<AVPacket*> audioPacketQueue_;
    std::mutex videoPacketMutex_;
    std::mutex audioPacketMutex_;
    std::condition_variable videoPacketCv_;
    std::condition_variable audioPacketCv_;
    std::atomic<bool> packetQueuesShutdown_{false};
};

} // namespace myplayer