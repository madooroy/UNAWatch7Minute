/**
 ******************************************************************************
 * @file    Theme.hpp
 * @brief   Colours and small formatting helpers shared by the screens.
 *
 * Colours come from the SDK's 64-colour palette, which the watch's
 * two-bits-per-channel display renders exactly.
 ******************************************************************************
 */

#ifndef THEME_HPP
#define THEME_HPP

#include <cstdint>
#include <cstdio>

#include "SDK/GUI/Color.hpp"

#include "WorkoutPlan.hpp"

namespace Theme
{
namespace C = SDK::GUI::Color;

constexpr uint32_t kText      = C::WHITE;
constexpr uint32_t kTextDim   = C::GRAY;
constexpr uint32_t kTrack     = C::GRAY_DARK;
constexpr uint32_t kWork      = C::GREEN;
constexpr uint32_t kRest      = C::CYAN_PURE;
constexpr uint32_t kReady     = C::YELLOW;
constexpr uint32_t kPaused    = C::GRAY;
constexpr uint32_t kDanger    = C::RED;
constexpr uint32_t kAccent    = C::YELLOW_DARK;   // amber, as the UNA apps' "select"
constexpr uint32_t kHeart     = C::RED;
constexpr uint32_t kDotTodo   = C::GRAY_DARK;

inline uint32_t categoryColor(Workout::Category c)
{
    switch (c) {
        case Workout::Category::TOTAL_BODY: return C::YELLOW_DARK;
        case Workout::Category::LOWER_BODY: return C::SKY;
        case Workout::Category::UPPER_BODY: return C::PINK_DARK;
        case Workout::Category::CORE:       return C::LIME;
    }
    return C::WHITE;
}

/// "m:ss" for a duration in ms (rounded down to the second).
inline void formatMinSec(char* buf, size_t len, uint32_t ms)
{
    const uint32_t s = ms / 1000;
    std::snprintf(buf, len, "%lu:%02lu", static_cast<unsigned long>(s / 60), static_cast<unsigned long>(s % 60));
}

} // namespace Theme

#endif // THEME_HPP
