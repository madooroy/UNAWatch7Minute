/**
 ******************************************************************************
 * @file    Commands.hpp
 * @brief   Service <-> GUI message contract of the 7-Minute app.
 *
 * The service owns the workout clock and every cue; the GUI only renders.
 * State is sent as an event-driven snapshot (on every phase change, pause,
 * skip and GUI start). Between snapshots the GUI extrapolates the countdown
 * from the shared kernel tick, so no per-second messages are needed:
 *
 *     running: remaining = endTick - now
 *     paused : remaining = remainingMs
 ******************************************************************************
 */

#ifndef COMMANDS_HPP
#define COMMANDS_HPP

#include <cstdint>

#include "SDK/Messages/MessageBase.hpp"
#include "SDK/Messages/MessageTypes.hpp"

#include "WorkoutPlan.hpp"

#pragma pack(push, 4)

namespace CustomMessage
{

// -- Message type ids (application-custom) ------------------------------------

// GUI --> Service
constexpr SDK::MessageType::Type WO_START         = 0x00000001;
constexpr SDK::MessageType::Type WO_CONTROL       = 0x00000002;
constexpr SDK::MessageType::Type WO_SETTINGS_SAVE = 0x00000003;

// Service --> GUI
constexpr SDK::MessageType::Type WO_STATE    = 0x00000010;
constexpr SDK::MessageType::Type WO_SETTINGS = 0x00000011;
constexpr SDK::MessageType::Type WO_HR       = 0x00000012;
constexpr SDK::MessageType::Type WO_SUMMARY  = 0x00000013;

enum class Control : uint8_t {
    TOGGLE_PAUSE = 0,
    PAUSE,
    RESUME,
    NEXT,
    PREV,
    FINISH,   ///< End the workout early and show the summary
    RESET,    ///< Leave the summary / abandon, back to the start screen
};

// -- GUI --> Service ----------------------------------------------------------

struct WorkoutStart : public SDK::MessageBase {
    uint8_t circuits;
    uint8_t alertMode;   // Workout::AlertMode
    WorkoutStart()
        : SDK::MessageBase(WO_START), circuits(1), alertMode(0)
    {}
    WorkoutStart(uint8_t c, Workout::AlertMode m)
        : WorkoutStart()
    {
        circuits  = c;
        alertMode = static_cast<uint8_t>(m);
    }
};

struct WorkoutControl : public SDK::MessageBase {
    Control cmd;
    WorkoutControl()
        : SDK::MessageBase(WO_CONTROL), cmd(Control::TOGGLE_PAUSE)
    {}
    explicit WorkoutControl(Control c)
        : WorkoutControl()
    {
        cmd = c;
    }
};

struct SettingsSave : public SDK::MessageBase {
    uint8_t circuits;
    uint8_t alertMode;
    SettingsSave()
        : SDK::MessageBase(WO_SETTINGS_SAVE), circuits(1), alertMode(0)
    {}
    SettingsSave(uint8_t c, Workout::AlertMode m)
        : SettingsSave()
    {
        circuits  = c;
        alertMode = static_cast<uint8_t>(m);
    }
};

// -- Service --> GUI ----------------------------------------------------------

struct WorkoutState : public SDK::MessageBase {
    uint8_t  phase;          // Workout::Phase
    uint8_t  paused;
    uint8_t  exercise;       // 0-based; the upcoming one during REST
    uint8_t  circuit;        // 1-based
    uint8_t  circuits;
    uint8_t  exercisesDone;
    uint32_t phaseDurationMs;
    uint32_t endTick;        // valid while running
    uint32_t remainingMs;    // at the moment of the snapshot
    WorkoutState()
        : SDK::MessageBase(WO_STATE)
        , phase(0), paused(0), exercise(0), circuit(1), circuits(1), exercisesDone(0)
        , phaseDurationMs(0), endTick(0), remainingMs(0)
    {}
};

struct Settings : public SDK::MessageBase {
    uint8_t  circuits;
    uint8_t  alertMode;
    uint16_t workoutsDone;   // lifetime count of completed workouts
    Settings()
        : SDK::MessageBase(WO_SETTINGS), circuits(1), alertMode(0), workoutsDone(0)
    {}
    Settings(uint8_t c, uint8_t m, uint16_t done)
        : Settings()
    {
        circuits     = c;
        alertMode    = m;
        workoutsDone = done;
    }
};

struct HeartRate : public SDK::MessageBase {
    uint8_t bpm;   // 0 = no reading
    HeartRate()
        : SDK::MessageBase(WO_HR), bpm(0)
    {}
    explicit HeartRate(uint8_t b)
        : HeartRate()
    {
        bpm = b;
    }
};

/// Result of a workout (payload only, so it can be kept and copied freely).
struct SummaryData {
    uint32_t activeMs         = 0;  // workout time, pauses excluded
    uint32_t workMs           = 0;  // time spent in work intervals
    uint16_t workoutsDone     = 0;  // lifetime count, this one included if completed
    uint8_t  exercisesDone    = 0;
    uint8_t  exercisesPlanned = 0;
    uint8_t  circuits         = 1;
    uint8_t  completed        = 0;
    uint8_t  avgHr            = 0;  // 0 = no readings
    uint8_t  maxHr            = 0;
};

struct Summary : public SDK::MessageBase {
    SummaryData data;
    Summary()
        : SDK::MessageBase(WO_SUMMARY), data{}
    {}
    explicit Summary(const SummaryData& d)
        : Summary()
    {
        data = d;
    }
};

} // namespace CustomMessage

#pragma pack(pop)

#endif // COMMANDS_HPP
