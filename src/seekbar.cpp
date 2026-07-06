/**
 * @file seekbar.cpp
 * @brief Seek bar implementation with SDL2 rendering and mouse interaction.
 */

#include "seekbar.h"
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace myplayer {

/**
 * @brief Construct a new SeekBar with default state.
 */
SeekBar::SeekBar() = default;

/**
 * @brief Set the current playback time and clamp to valid range.
 */
void SeekBar::SetCurrentTime(double currentTime) {
    currentTime_ = std::max(0.0, std::min(currentTime, duration_));
}

/**
 * @brief Set the total duration of the media.
 */
void SeekBar::SetDuration(double duration) {
    duration_ = std::max(0.0, duration);
}

/**
 * @brief Render the seek bar background, progress fill, and time text.
 */
void SeekBar::Render(SDL_Renderer* renderer, int windowWidth, int windowHeight) {
    if (!visible_ || !renderer) {
        return;
    }

    SDL_Rect barRect = ComputeBarRect(windowWidth, windowHeight);
    SDL_Rect fillRect = ComputeFillRect(barRect);

    // Draw background bar.
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 200);
    SDL_RenderFillRect(renderer, &barRect);

    // Draw progress fill.
    SDL_SetRenderDrawColor(renderer, 0, 150, 255, 255);
    SDL_RenderFillRect(renderer, &fillRect);

    // Draw border.
    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
    SDL_RenderDrawRect(renderer, &barRect);

    // Draw time text (simple representation via filled rectangles for now).
    // In a production app, you would use SDL_ttf for text rendering.
    // Here we draw a small indicator dot at the current position.
    if (duration_ > 0) {
        int dotX = barRect.x + fillRect.w;
        int dotY = barRect.y + barRect.h / 2;
        SDL_Rect dotRect = { dotX - 4, dotY - 6, 8, 12 };
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &dotRect);
    }
}

/**
 * @brief Handle mouse events for seek bar interaction.
 */
bool SeekBar::HandleMouseEvent(const SDL_Event& event, int windowWidth, int windowHeight) {
    if (!visible_) {
        return false;
    }

    SDL_Rect barRect = ComputeBarRect(windowWidth, windowHeight);

    switch (event.type) {
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                if (mx >= barRect.x && mx <= barRect.x + barRect.w &&
                    my >= barRect.y && my <= barRect.y + barRect.h) {
                    dragging_ = true;
                    double ratio = static_cast<double>(mx - barRect.x) / barRect.w;
                    double targetTime = ratio * duration_;
                    if (seekCallback_) {
                        seekCallback_(targetTime);
                    }
                    return true;
                }
            }
            break;

        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                dragging_ = false;
            }
            break;

        case SDL_MOUSEMOTION:
            if (dragging_) {
                int mx = event.motion.x;
                mx = std::clamp(mx, barRect.x, barRect.x + barRect.w);
                double ratio = static_cast<double>(mx - barRect.x) / barRect.w;
                double targetTime = ratio * duration_;
                if (seekCallback_) {
                    seekCallback_(targetTime);
                }
                return true;
            }
            break;

        default:
            break;
    }

    return false;
}

/**
 * @brief Format seconds into a human-readable time string.
 */
std::string SeekBar::FormatTime(double seconds) {
    int totalSeconds = static_cast<int>(seconds);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int secs = totalSeconds % 60;

    std::ostringstream oss;
    if (hours > 0) {
        oss << std::setfill('0') << std::setw(2) << hours << ':';
    }
    oss << std::setfill('0') << std::setw(2) << minutes << ':'
        << std::setfill('0') << std::setw(2) << secs;
    return oss.str();
}

/**
 * @brief Compute the seek bar's screen rectangle.
 */
SDL_Rect SeekBar::ComputeBarRect(int windowWidth, int windowHeight) const {
    int barW = std::max(100, windowWidth - 2 * kBarMarginHorizontal);
    int barX = (windowWidth - barW) / 2;
    int barY = windowHeight - kBarMarginBottom - kBarHeight;
    return { barX, barY, barW, kBarHeight };
}

/**
 * @brief Compute the filled portion of the progress bar.
 */
SDL_Rect SeekBar::ComputeFillRect(const SDL_Rect& barRect) const {
    if (duration_ <= 0) {
        return { barRect.x, barRect.y, 0, barRect.h };
    }
    double ratio = std::clamp(currentTime_ / duration_, 0.0, 1.0);
    int fillW = static_cast<int>(barRect.w * ratio);
    return { barRect.x, barRect.y, fillW, barRect.h };
}

} // namespace myplayer