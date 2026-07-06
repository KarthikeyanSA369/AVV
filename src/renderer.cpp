/**
 * @file renderer.cpp
 * @brief SDL2 video renderer implementation.
 */

#include "renderer.h"
#include <iostream>
#include <algorithm>

extern "C" {
#include <libavutil/imgutils.h>
}

namespace myplayer {

/**
 * @brief Construct a new Renderer.
 */
Renderer::Renderer() = default;

/**
 * @brief Destructor. Ensures clean shutdown.
 */
Renderer::~Renderer() {
    Shutdown();
}

/**
 * @brief Initialize SDL video subsystem, create window and renderer.
 */
bool Renderer::Initialize(const std::string& title, int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_.load()) {
        return true;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return false;
    }

    window_ = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << std::endl;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return false;
    }

    renderer_ = SDL_CreateRenderer(
        window_,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!renderer_) {
        // Fallback to software renderer.
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer_) {
            std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            return false;
        }
    }

    windowWidth_ = width;
    windowHeight_ = height;
    initialized_.store(true);
    return true;
}

/**
 * @brief Shutdown SDL and free all rendering resources.
 */
void Renderer::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }

    rgbFrame_.reset();
    rgbBuffer_.clear();

    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    initialized_.store(false);
}

/**
 * @brief Render a decoded video frame to the SDL window.
 */
bool Renderer::RenderFrame(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!initialized_.load() || !frame.frame) {
        return false;
    }

    AVFrame* srcFrame = frame.frame.get();
    int srcW = srcFrame->width;
    int srcH = srcFrame->height;
    AVPixelFormat srcFmt = static_cast<AVPixelFormat>(srcFrame->format);

    // Create or update texture if dimensions/format changed.
    if (!texture_ || textureWidth_ != srcW || textureHeight_ != srcH || textureFormat_ != srcFmt) {
        if (!CreateTexture(srcW, srcH, srcFmt)) {
            return false;
        }
    }

    // Upload frame data to SDL texture.
    if (srcFmt == AV_PIX_FMT_YUV420P || srcFmt == AV_PIX_FMT_YUVJ420P) {
        // Direct YUV upload for SDL_PIXELFORMAT_IYUV.
        SDL_UpdateYUVTexture(
            texture_,
            nullptr,
            srcFrame->data[0], srcFrame->linesize[0],  // Y plane
            srcFrame->data[1], srcFrame->linesize[1],  // U plane
            srcFrame->data[2], srcFrame->linesize[2]   // V plane
        );
    } else {
        // Convert to RGBA using swscale, then upload.
        if (!swsCtx_) {
            swsCtx_ = sws_getContext(
                srcW, srcH, srcFmt,
                srcW, srcH, AV_PIX_FMT_RGBA,
                SWS_BILINEAR, nullptr, nullptr, nullptr
            );
            if (!swsCtx_) {
                return false;
            }

            rgbFrame_.reset(av_frame_alloc());
            int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, srcW, srcH, 1);
            rgbBuffer_.resize(bufSize);
            av_image_fill_arrays(
                rgbFrame_->data, rgbFrame_->linesize,
                rgbBuffer_.data(), AV_PIX_FMT_RGBA, srcW, srcH, 1
            );
        }

        sws_scale(
            swsCtx_,
            srcFrame->data, srcFrame->linesize,
            0, srcH,
            rgbFrame_->data, rgbFrame_->linesize
        );

        SDL_UpdateTexture(
            texture_,
            nullptr,
            rgbFrame_->data[0],
            rgbFrame_->linesize[0]
        );
    }

    // Clear and render.
    SDL_RenderClear(renderer_);
    SDL_Rect dstRect = ComputeDestinationRect(srcW, srcH);
    SDL_RenderCopy(renderer_, texture_, nullptr, &dstRect);
    SDL_RenderPresent(renderer_);

    return true;
}

/**
 * @brief Clear the renderer to black.
 */
void Renderer::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (renderer_) {
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderPresent(renderer_);
    }
}

/**
 * @brief Present the current render buffer.
 */
void Renderer::Present() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (renderer_) {
        SDL_RenderPresent(renderer_);
    }
}

/**
 * @brief Handle window resize events.
 */
void Renderer::Resize(int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    windowWidth_ = width;
    windowHeight_ = height;
}

/**
 * @brief Create an SDL texture matching the video frame format.
 */
bool Renderer::CreateTexture(int width, int height, AVPixelFormat format) {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }

    Uint32 sdlFormat = SDL_PIXELFORMAT_UNKNOWN;
    if (format == AV_PIX_FMT_YUV420P || format == AV_PIX_FMT_YUVJ420P) {
        sdlFormat = SDL_PIXELFORMAT_IYUV;
    } else {
        sdlFormat = SDL_PIXELFORMAT_RGBA32;
    }

    texture_ = SDL_CreateTexture(
        renderer_,
        sdlFormat,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );

    if (!texture_) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << std::endl;
        return false;
    }

    textureWidth_ = width;
    textureHeight_ = height;
    textureFormat_ = format;

    // Reset swscale context since format may have changed.
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }
    rgbFrame_.reset();
    rgbBuffer_.clear();

    return true;
}

/**
 * @brief Compute destination rectangle preserving aspect ratio.
 */
SDL_Rect Renderer::ComputeDestinationRect(int srcWidth, int srcHeight) const {
    if (!keepAspectRatio_ || srcWidth <= 0 || srcHeight <= 0) {
        return {0, 0, windowWidth_, windowHeight_};
    }

    float srcAspect = static_cast<float>(srcWidth) / static_cast<float>(srcHeight);
    float dstAspect = static_cast<float>(windowWidth_) / static_cast<float>(windowHeight_);

    int dstW, dstH;
    if (srcAspect > dstAspect) {
        dstW = windowWidth_;
        dstH = static_cast<int>(windowWidth_ / srcAspect);
    } else {
        dstH = windowHeight_;
        dstW = static_cast<int>(windowHeight_ * srcAspect);
    }

    int x = (windowWidth_ - dstW) / 2;
    int y = (windowHeight_ - dstH) / 2;

    return {x, y, dstW, dstH};
}

} // namespace myplayer