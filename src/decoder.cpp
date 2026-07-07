#include "decoder.h"
#include <iostream>

namespace myplayer {

// ---------------------------
// FrameQueue<T> definitions
// ---------------------------

template <typename T>
bool FrameQueue<T>::Push(T frame) {
    std::unique_lock<std::mutex> lk(mutex_);
    notFull_.wait(lk, [this]() { return queue_.size() < kMaxFrameQueueSize || shutdown_.load(); });
    if (shutdown_.load()) {
        return false;
    }
    queue_.push(std::move(frame));
    notEmpty_.notify_one();
    return true;
}

template <typename T>
bool FrameQueue<T>::Pop(T& frame) {
    std::unique_lock<std::mutex> lk(mutex_);
    notEmpty_.wait(lk, [this]() { return !queue_.empty() || shutdown_.load(); });
    if (queue_.empty()) {
        return false;
    }
    frame = std::move(queue_.front());
    queue_.pop();
    notFull_.notify_one();
    return true;
}

template <typename T>
size_t FrameQueue<T>::Size() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return queue_.size();
}

template <typename T>
void FrameQueue<T>::Clear() {
    std::lock_guard<std::mutex> lk(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

template <typename T>
void FrameQueue<T>::Shutdown() {
    shutdown_.store(true);
    notEmpty_.notify_all();
    notFull_.notify_all();
}

template <typename T>
bool FrameQueue<T>::IsShutdown() const {
    return shutdown_.load();
}

// Explicit instantiations for the frame types used in the project.
template class FrameQueue<VideoFrame>;
template class FrameQueue<AudioFrame>;

// ---------------------------
// Decoder implementation
// ---------------------------

Decoder::Decoder() = default;

Decoder::~Decoder() {
    Close();
}

bool Decoder::Open(const std::string& filepath) {
    // Minimal stub implementation: open is not implemented in this lightweight build.
    // A full implementation should use FFmpeg APIs (avformat_open_input, avcodec_open2, etc.).
    std::cerr << "Decoder::Open called for '" << filepath << "' - not implemented in this build." << std::endl;
    return false;
}

void Decoder::Close() {
    Stop();

    if (videoCodecCtx_) {
        avcodec_free_context(&videoCodecCtx_);
        videoCodecCtx_ = nullptr;
    }
    if (audioCodecCtx_) {
        avcodec_free_context(&audioCodecCtx_);
        audioCodecCtx_ = nullptr;
    }
    if (fmtCtx_) {
        avformat_close_input(&fmtCtx_);
        fmtCtx_ = nullptr;
    }

    // Free any remaining packets in packet queues
    {
        std::lock_guard<std::mutex> lock(videoPacketMutex_);
        while (!videoPacketQueue_.empty()) {
            AVPacket* pkt = videoPacketQueue_.front();
            av_packet_free(&pkt);
            videoPacketQueue_.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lock(audioPacketMutex_);
        while (!audioPacketQueue_.empty()) {
            AVPacket* pkt = audioPacketQueue_.front();
            av_packet_free(&pkt);
            audioPacketQueue_.pop();
        }
    }

    videoQueue_.Clear();
    audioQueue_.Clear();
}

void Decoder::Start() {
    if (running_.load()) {
        return;
    }

    running_.store(true);
    packetQueuesShutdown_.store(false);

    // In the real implementation these threads would be started to demux and decode.
    // For now we create no threads in this stub.
}

void Decoder::Stop() {
    if (!running_.load()) {
        // Ensure queues are signaled to unblock any waiters.
        packetQueuesShutdown_.store(true);
        videoQueue_.Shutdown();
        audioQueue_.Shutdown();
        return;
    }

    running_.store(false);
    packetQueuesShutdown_.store(true);

    // Join threads if they were running.
    if (demuxThread_.joinable()) demuxThread_.join();
    if (videoDecodeThread_.joinable()) videoDecodeThread_.join();
    if (audioDecodeThread_.joinable()) audioDecodeThread_.join();

    // Clear packet queues
    {
        std::lock_guard<std::mutex> lock(videoPacketMutex_);
        while (!videoPacketQueue_.empty()) {
            AVPacket* pkt = videoPacketQueue_.front();
            av_packet_free(&pkt);
            videoPacketQueue_.pop();
        }
    }
    {
        std::lock_guard<std::mutex> lock(audioPacketMutex_);
        while (!audioPacketQueue_.empty()) {
            AVPacket* pkt = audioPacketQueue_.front();
            av_packet_free(&pkt);
            audioPacketQueue_.pop();
        }
    }

    videoQueue_.Shutdown();
    audioQueue_.Shutdown();
}

bool Decoder::Seek(double /*targetTime*/) {
    // Not implemented in stub.
    return false;
}

int Decoder::DecodePacket(AVCodecContext* /*codecCtx*/, AVPacket* /*packet*/, int /*streamIndex*/) {
    // Stub: decoding not implemented here.
    return 0;
}

void Decoder::FlushCodec(AVCodecContext* /*codecCtx*/, bool /*isVideo*/) {
    // Stub: flush logic would go here.
}

void Decoder::DemuxThread() {
    // Stub: demuxing loop would go here.
}

void Decoder::VideoDecodeThread() {
    // Stub: video decode loop would go here.
}

void Decoder::AudioDecodeThread() {
    // Stub: audio decode loop would go here.
}

} // namespace myplayer
