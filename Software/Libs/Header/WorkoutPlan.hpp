/**
 ******************************************************************************
 * @file    WorkoutPlan.hpp
 * @brief   The 7-minute high-intensity circuit (HICT) as published by
 *          Klika & Jordan, "High-intensity circuit training using body weight:
 *          Maximum results with minimum investment", ACSM's Health & Fitness
 *          Journal 17(3):8-13, 2013 (popularised by the NYT "Scientific
 *          7-Minute Workout", May 2013).
 *
 * Protocol: 12 exercises, 30 s of work each, 10 s transition between them,
 * opposing muscle groups alternating (total body / lower / upper / core).
 * One circuit is ~7-8 minutes; the authors suggest repeating it 2-3 times.
 *
 * Pure data, no SDK dependency: shared by the service, the GUI and the host
 * unit tests.
 ******************************************************************************
 */

#ifndef WORKOUT_PLAN_HPP
#define WORKOUT_PLAN_HPP

#include <cstdint>

namespace Workout
{

/// Muscle-group category; the circuit cycles through them in this order.
enum class Category : uint8_t {
    TOTAL_BODY = 0,
    LOWER_BODY,
    UPPER_BODY,
    CORE,
};

struct Exercise {
    const char* name;        ///< Short on-watch name (fits the 240 px face)
    const char* tip;         ///< One-line coaching cue shown under the name
    Category    category;
    bool        switchSides; ///< Cue a side switch at the half-way point
};

static constexpr uint8_t  kExerciseCount = 12;
static constexpr uint32_t kWorkMs        = 30000;  ///< Work interval
static constexpr uint32_t kRestMs        = 10000;  ///< Transition interval
static constexpr uint32_t kGetReadyMs    = 10000;  ///< Lead-in before exercise 1
static constexpr uint8_t  kMinCircuits   = 1;
static constexpr uint8_t  kMaxCircuits   = 3;
static constexpr uint8_t  kCountdownSecs = 3;      ///< Ticks in the last N seconds

// clang-format off
static constexpr Exercise kExercises[kExerciseCount] = {
    { "Jumping Jacks",  "Steady rhythm",   Category::TOTAL_BODY, false },
    { "Wall Sit",       "Thighs parallel", Category::LOWER_BODY, false },
    { "Push-Ups",       "Knees to scale",  Category::UPPER_BODY, false },
    { "Crunches",       "Slow & steady",   Category::CORE,       false },
    { "Step-Ups",       "Chair, alt legs", Category::TOTAL_BODY, false },
    { "Squats",         "Hips back",       Category::LOWER_BODY, false },
    { "Triceps Dips",   "Use a chair",     Category::UPPER_BODY, false },
    { "Plank",          "Brace your core", Category::CORE,       false },
    { "High Knees",     "Run in place",    Category::TOTAL_BODY, false },
    { "Lunges",         "Alternate legs",  Category::LOWER_BODY, false },
    { "Push-Up Rotate", "Alternate sides", Category::UPPER_BODY, false },
    { "Side Plank",     "15s each side",   Category::CORE,       true  },
};
// clang-format on

inline const char* categoryName(Category c)
{
    switch (c) {
        case Category::TOTAL_BODY: return "TOTAL BODY";
        case Category::LOWER_BODY: return "LOWER BODY";
        case Category::UPPER_BODY: return "UPPER BODY";
        case Category::CORE:       return "CORE";
    }
    return "";
}

/// Planned length of a workout of @p circuits, lead-in included, in ms.
inline constexpr uint32_t plannedDurationMs(uint8_t circuits)
{
    return kGetReadyMs
         + static_cast<uint32_t>(circuits) * kExerciseCount * kWorkMs
         + (static_cast<uint32_t>(circuits) * kExerciseCount - 1u) * kRestMs;
}

/// How the cues are delivered.
enum class AlertMode : uint8_t {
    BEEP_AND_VIBRO = 0,
    VIBRO_ONLY,
    BEEP_ONLY,
    COUNT
};

inline const char* alertModeName(AlertMode m)
{
    switch (m) {
        case AlertMode::BEEP_AND_VIBRO: return "Beep + Buzz";
        case AlertMode::VIBRO_ONLY:     return "Buzz only";
        case AlertMode::BEEP_ONLY:      return "Beep only";
        default:                        return "";
    }
}

} // namespace Workout

#endif // WORKOUT_PLAN_HPP
