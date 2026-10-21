/**
 ******************************************************************************
 * @file    WorkoutEngine.hpp
 * @brief   Tick-driven state machine that walks through the 7-minute circuit
 *          and decides when each audible/haptic cue is due.
 *
 * The engine has no SDK dependency and no clock of its own: every call takes
 * the current kernel tick in milliseconds, and execute() returns how long the
 * caller may sleep before the next thing can happen. That keeps it exact
 * (phases are chained from the previous phase's end tick, so a late wake-up
 * never makes the workout drift) and lets the host unit tests drive it with a
 * fake clock. All tick arithmetic is wrap-safe.
 *
 * Timeline for N circuits:
 *
 *   GET_READY 10s -> WORK ex1 30s -> REST 10s -> WORK ex2 -> ... -> WORK ex12
 *      -> (REST -> WORK ex1 of the next circuit ...) -> DONE
 *
 * While in REST, exercise()/circuit() name the *upcoming* exercise, so the
 * screen can preview it.
 ******************************************************************************
 */

#ifndef WORKOUT_ENGINE_HPP
#define WORKOUT_ENGINE_HPP

#include <cstdint>

#include "WorkoutPlan.hpp"

namespace Workout
{

enum class Phase : uint8_t {
    IDLE = 0,
    GET_READY,
    WORK,
    REST,
    DONE,
};

enum class Cue : uint8_t {
    COUNTDOWN = 0,  ///< One of the last 3 seconds of a phase; arg = 3, 2 or 1
    GO,             ///< A work interval starts
    REST,           ///< A work interval ended, transition starts
    SWITCH_SIDES,   ///< Half-way through a two-sided exercise
    CIRCUIT_DONE,   ///< A circuit ended and another follows; arg = circuits done
    FINISHED,       ///< The last work interval ended
    PAUSED,
    RESUMED,
};

class Engine
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void onCue(Cue cue, uint8_t arg) = 0;
        /// Phase, exercise, pause state or timing changed: resend the snapshot.
        virtual void onStateChanged() = 0;
    };

    /// Returned by execute() when nothing is scheduled.
    static constexpr uint32_t kNoWake = 0xFFFFFFFFu;

    void setListener(Listener* listener) { mListener = listener; }

    // -- Commands -------------------------------------------------------------

    void start(uint8_t circuits, uint32_t now);
    void pause(uint32_t now);
    void resume(uint32_t now);
    void togglePause(uint32_t now);
    /// Skip forward: lead-in -> exercise 1, work -> next rest, rest -> work.
    void next(uint32_t now);
    /// Skip back: restart the current exercise, or (within its first 3 s, or
    /// from a rest) go back to the previous one.
    void prev(uint32_t now);
    /// End early. Moves to DONE with completed() == false.
    void finish(uint32_t now);
    /// Leave DONE (or abandon a workout) and return to IDLE.
    void reset();

    /**
     * @brief Advance phases and fire due cues.
     * @return ms until the next call is needed, or kNoWake when idle/paused.
     */
    uint32_t execute(uint32_t now);

    // -- State ----------------------------------------------------------------

    Phase    phase()    const { return mPhase; }
    bool     paused()   const { return mPaused; }
    bool     active()   const { return mPhase == Phase::GET_READY || mPhase == Phase::WORK || mPhase == Phase::REST; }
    uint8_t  exercise() const { return mExercise; }   ///< 0-based
    uint8_t  circuit()  const { return mCircuit; }    ///< 1-based
    uint8_t  circuits() const { return mCircuits; }
    uint32_t endTick()  const { return mEndTick; }
    uint32_t phaseDurationMs() const { return durationOf(mPhase); }
    uint32_t remainingMs(uint32_t now) const;

    /// Time spent in the workout, pauses excluded.
    uint32_t activeMs(uint32_t now) const;
    /// Accumulated time in WORK intervals, pauses excluded.
    uint32_t workMs(uint32_t now) const;
    /// Work intervals that ran to the end (skipped ones are not counted).
    uint16_t exercisesDone() const { return mExercisesDone; }
    /// True when the last interval ran out and >= 75% of the intervals were done (not skipped).
    bool     completed() const { return mCompleted; }

    static uint32_t durationOf(Phase p);

private:
    Listener* mListener = nullptr;

    Phase    mPhase    = Phase::IDLE;
    bool     mPaused   = false;
    uint8_t  mExercise = 0;
    uint8_t  mCircuit  = 1;
    uint8_t  mCircuits = 1;

    uint32_t mPhaseStart      = 0;  ///< Tick the phase started (shifted on resume)
    uint32_t mEndTick         = 0;  ///< Tick the phase ends (valid while running)
    uint32_t mPausedRemaining = 0;  ///< Remainder frozen while paused

    uint32_t mRunStart     = 0;     ///< Tick the current un-paused stretch began
    uint32_t mActiveBase   = 0;     ///< Active ms banked before mRunStart
    uint32_t mWorkBase     = 0;     ///< Work ms banked (closed work stretches)
    uint32_t mWorkRunStart = 0;     ///< Tick the current work stretch began

    uint16_t mExercisesDone = 0;
    bool     mCompleted     = false;

    uint8_t  mLastCountdown = 0;    ///< Last countdown second cued in this phase
    bool     mSwitchCued    = false;

    void enter(Phase phase, uint8_t exercise, uint8_t circuit, uint32_t startTick, bool cue);
    void advance(uint32_t startTick, bool natural);
    void bankWork(uint32_t now);
    void bankActive(uint32_t now);
    void cue(Cue c, uint8_t arg = 0);
    void changed();

    static int32_t diff(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b); }
};

} // namespace Workout

#endif // WORKOUT_ENGINE_HPP
