/**
 * @file seekbar.h
 * @brief Interactive seek bar overlay rendered via SDL2.
 *
 * Displays a progress bar, current time, total duration, and handles
 * mouse interaction for seeking.
 */

#pragma once

#include <SDL.h>
#include <string>
#include <functional>

namespace myplayer {

/**
 * @brief Interactive seek/progress bar UI element.
 */
class SeekBar {
public:
    /**
     * @brief Callback type for seek requests.
     * @param targetTime The time to seek to, in seconds.
     */
    using SeekCallback = std::function<void(double)>;

    /**
     * @brief Construct a new SeekBar.
     */
    SeekBar();

    /**
     * @brief Set the seek callback function.
     * @param callback Function to call when user seeks.
     */
    void SetSeekCallback(SeekCallback callback) { seekCallback_ = std::move(callback); }

    /**
     * @brief Set the current playback position.
     * @param currentTime Current time in seconds.
     */
    void SetCurrentTime(double currentTime);

    /**
     * @brief Set the total media duration.
     * @param duration Total duration in seconds.
     */
    void SetDuration(double duration);

    /**
     * @brief Render the seek bar to the given SDL renderer.
     * @param renderer The SDL renderer.
     * @param windowWidth Current window width.
     * @param windowHeight Current window height.
     */
    void Render(SDL_Renderer* renderer, int windowWidth, int windowHeight);

    /**
     * @brief Handle an SDL mouse event.
     * @param event The SDL mouse event.
     * @param windowWidth Current window width.
     * @param windowHeight Current window height.
     * @return true if the event was consumed.
     */
    bool HandleMouseEvent(const SDL_Event& event, int windowWidth, int windowHeight);

    /**
     * @brief Show or hide the seek bar.
     * @param visible true to show, false to hide.
     */
    void SetVisible(bool visible) { visible_ = visible; }

    /**
     * @brief Check if the seek bar is visible.
     */
    bool IsVisible() const { return visible_; }

private:
    /**
     * @brief Format a time value (seconds) as HH:MM:SS or MM:SS.
     * @param seconds Time in seconds.
     * @return Formatted string.
     */
    static std::string FormatTime(double seconds);

    /**
     * @brief Compute the seek bar's screen rectangle.
     * @param windowWidth Window width.
     * @param windowHeight Window height.
     * @return SDL_Rect representing the bar area.
     */
    SDL_Rect ComputeBarRect(int windowWidth, int windowHeight) const;

    /**
     * @brief Compute the progress fill rectangle.
     * @param barRect The full bar rectangle.
     * @return SDL_Rect representing the filled portion.
     */
    SDL_Rect ComputeFillRect(const SDL_Rect& barRect) const;

    SeekCallback seekCallback_;
    double currentTime_{0.0};
    double duration_{0.0};
    bool visible_{true};
    bool dragging_{false};

    // Visual constants.
    static constexpr int kBarHeight = 24;
    static constexpr int kBarMarginBottom = 40;
    static constexpr int kBarMarginHorizontal = 40;
};

} // namespace myplayer