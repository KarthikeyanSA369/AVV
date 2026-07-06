/**
 * @file renderer.h
 * @brief SDL2-based video renderer with YUV-to-RGB conversion.
 *
 * Renders decoded video frames to an SDL2 window using hardware-accelerated
 * or software texture scaling.
 */

#pragma once

#include "decoder.h"
#include <string>
#include <memory>
#include <atomic>
#include <mutex>

#include <SDL.h>

namespace myplayer {

/**
 * @brief Video renderer using SDL2 for window management and texture rendering.
 */
class Renderer {
public:
    /**
     * @brief Construct a new Renderer.
     */
    Renderer();

    /**
     * @brief Destructor. Cleans up all SDL resources.
     */
    ~Renderer();

    // Non-copyable, non-movable.
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    /**
     * @brief Initialize the SDL window and renderer.
     * @param title Window title.
     * @param width Initial window width.
     * @param height Initial window height.
     * @return true on success, false on failure.
     */
    bool Initialize(const std::string& title, int width, int height);

    /**
     * @brief Shutdown and release all SDL resources.
     */
    void Shutdown();

    /**
     * @brief Render a single video frame to the screen.
     * @param frame The decoded video frame to render.
     * @return true on success, false on failure.
     */
    bool RenderFrame(const VideoFrame& frame);

    /**
     * @brief Clear the screen to black.
     */
    void Clear();

    /**
     * @brief Present the current frame buffer to the display.
     */
    void Present();

    /**
     * @brief Resize the rendering viewport.
     * @param width New width.
     * @param height New height.
     */
    void Resize(int width, int height);

    /**
     * @brief Get the current window width.
     */
    int GetWindowWidth() const { return windowWidth_; }

    /**
     * @brief Get the current window height.
     */
    int GetWindowHeight() const { return windowHeight_; }

    /**
     * @brief Get the SDL window handle (for event processing).
     */
    SDL_Window* GetWindow() const { return window_; }

    /**
     * @brief Check if the renderer is initialized.
     */
    bool IsInitialized() const { return initialized_.load(); }

    /**
     * @brief Set the display aspect ratio correction mode.
     * @param keepAspect true to preserve original aspect ratio, false to stretch.
     */
    void SetKeepAspectRatio(bool keepAspect) { keepAspectRatio_ = keepAspect; }

private:
    /**
     * @brief Create or recreate the SDL texture for the given frame dimensions.
     * @param width Frame width.
     * @param height Frame height.
     * @param format Pixel format of the incoming frames.
     * @return true on success.
     */
    bool CreateTexture(int width, int height, AVPixelFormat format);

    /**
     * @brief Compute the destination rectangle for aspect-ratio-correct rendering.
     * @param srcWidth Original video width.
     * @param srcHeight Original video height.
     * @return SDL_Rect representing the centered, scaled destination.
     */
    SDL_Rect ComputeDestinationRect(int srcWidth, int srcHeight) const;

    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    SDL_Texture* texture_{nullptr};

    // Software scaler for YUV-to-RGB conversion.
    struct SwsContext* swsCtx_{nullptr};
    std::unique_ptr<AVFrame, decltype(&av_frame_unref)> rgbFrame_{nullptr, av_frame_unref};
    std::vector<uint8_t> rgbBuffer_;

    int windowWidth_{0};
    int windowHeight_{0};
    int textureWidth_{0};
    int textureHeight_{0};
    AVPixelFormat textureFormat_{AV_PIX_FMT_NONE};

    std::atomic<bool> initialized_{false};
    bool keepAspectRatio_{true};
    mutable std::mutex mutex_;
};

} // namespace myplayer