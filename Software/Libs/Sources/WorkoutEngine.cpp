/**
 ******************************************************************************
 * @file    WorkoutEngine.cpp
 * @brief   7-minute circuit state machine (see WorkoutEngine.hpp).
 ******************************************************************************
 */

#include "WorkoutEngine.hpp"

namespace Workout
{

namespace
{
/// Skip-back within this much of a work interval's start goes to the
/// previous exercise instead of restarting the current one.
constexpr uint32_t kPrevGraceMs = 3000;
} // namespace

uint32_t Engine::durationOf(Phase p)
{
    switch (p) {
        case Phase::GET_READY: return kGetReadyMs;
        case Phase::WORK:      return kWorkMs;
        case Phase::REST:      return kRestMs;
        default:               return 0;
    }
}

// -- Commands -----------------------------------------------------------------

void Engine::start(uint8_t circuits, uint32_t now)
{
    if (circuits < kMinCircuits) circuits = kMinCircuits;
    if (circuits > kMaxCircuits) circuits = kMaxCircuits;

    mCircuits      = circuits;
    mPaused        = false;
    mActiveBase    = 0;
    mWorkBase      = 0;
    mRunStart      = now;
    mExercisesDone = 0;
    mCompleted     = false;

    enter(Phase::GET_READY, 0, 1, now, false);
    changed();
}

void Engine::pause(uint32_t now)
{
    if (!active() || mPaused) {
        return;
    }
    mPausedRemaining = remainingMs(now);
    bankActive(now);
    if (mPhase == Phase::WORK) {
        bankWork(now);
    }
    mPaused = true;
    cue(Cue::PAUSED);
    changed();
}

void Engine::resume(uint32_t now)
{
    if (!active() || !mPaused) {
        return;
    }
    mPaused     = false;
    mRunStart   = now;
    mEndTick    = now + mPausedRemaining;
    mPhaseStart = mEndTick - durationOf(mPhase);
    if (mPhase == Phase::WORK) {
        mWorkRunStart = now;
    }
    cue(Cue::RESUMED);
    changed();
}

void Engine::togglePause(uint32_t now)
{
    if (mPaused) {
        resume(now);
    } else {
        pause(now);
    }
}

void Engine::next(uint32_t now)
{
    if (!active()) {
        return;
    }
    if (mPhase == Phase::WORK && !mPaused) {
        bankWork(now);
    }
    advance(now, false);
    changed();
}

void Engine::prev(uint32_t now)
{
    if (!active()) {
        return;
    }
    if (mPhase == Phase::WORK && !mPaused) {
        bankWork(now);
    }

    const uint32_t elapsed = durationOf(mPhase) - remainingMs(now);

    switch (mPhase) {
        case Phase::GET_READY:
            enter(Phase::GET_READY, 0, 1, now, false);
            break;

        case Phase::WORK:
            if (elapsed > kPrevGraceMs || (mExercise == 0 && mCircuit == 1)) {
                enter(Phase::WORK, mExercise, mCircuit, now, true);   // restart
                break;
            }
            [[fallthrough]];

        case Phase::REST: {
            // Back to the exercise before the current/upcoming one.
            uint8_t ex = mExercise;
            uint8_t c  = mCircuit;
            if (ex == 0) {
                if (c > 1) {
                    --c;
                    ex = kExerciseCount - 1;
                }
            } else {
                --ex;
            }
            enter(Phase::WORK, ex, c, now, true);
        } break;

        default:
            break;
    }
    changed();
}

void Engine::finish(uint32_t now)
{
    if (!active()) {
        return;
    }
    if (!mPaused) {
        bankActive(now);
        if (mPhase == Phase::WORK) {
            bankWork(now);
        }
    }
    mPaused    = false;
    mCompleted = false;
    mPhase     = Phase::DONE;
    changed();
}

void Engine::reset()
{
    mPhase    = Phase::IDLE;
    mPaused   = false;
    mExercise = 0;
    mCircuit  = 1;
    changed();
}

// -- Clock --------------------------------------------------------------------

uint32_t Engine::execute(uint32_t now)
{
    if (!active() || mPaused) {
        return kNoWake;
    }

    // Chain phases from the previous end tick so a late wake never drifts the
    // schedule. A bounded loop covers a wake that is late by several phases.
    for (int guard = 0; guard < 4 && active() && diff(mEndTick, now) <= 0; ++guard) {
        if (mPhase == Phase::WORK) {
            bankWork(mEndTick);
        }
        advance(mEndTick, true);
        changed();
    }
    if (!active()) {
        return kNoWake;
    }

    const uint32_t remaining = remainingMs(now);

    // Countdown ticks in the last kCountdownSecs seconds of every phase.
    const uint8_t sec = static_cast<uint8_t>((remaining + 999) / 1000);
    if (sec >= 1 && sec <= kCountdownSecs && sec != mLastCountdown) {
        mLastCountdown = sec;
        cue(Cue::COUNTDOWN, sec);
    }

    // Two-sided exercise: switch sides half-way.
    uint32_t wake = remaining % 1000u;
    if (wake == 0) {
        wake = 1000;
    }
    if (mPhase == Phase::WORK && kExercises[mExercise].switchSides && !mSwitchCued) {
        const uint32_t half = kWorkMs / 2;
        if (remaining <= half) {
            mSwitchCued = true;
            cue(Cue::SWITCH_SIDES);
        } else if (remaining - half < wake) {
            wake = remaining - half;
        }
    }
    if (wake > remaining) {
        wake = remaining;
    }
    return wake;
}

uint32_t Engine::remainingMs(uint32_t now) const
{
    if (!active()) {
        return 0;
    }
    if (mPaused) {
        return mPausedRemaining;
    }
    const int32_t r = diff(mEndTick, now);
    return r > 0 ? static_cast<uint32_t>(r) : 0;
}

uint32_t Engine::activeMs(uint32_t now) const
{
    if (active() && !mPaused) {
        return mActiveBase + (now - mRunStart);
    }
    return mActiveBase;
}

uint32_t Engine::workMs(uint32_t now) const
{
    if (mPhase == Phase::WORK && !mPaused) {
        return mWorkBase + (now - mWorkRunStart);
    }
    return mWorkBase;
}

// -- Internals ----------------------------------------------------------------

void Engine::enter(Phase phase, uint8_t exercise, uint8_t circuit, uint32_t startTick, bool withCue)
{
    mPhase         = phase;
    mExercise      = exercise;
    mCircuit       = circuit;
    mPhaseStart    = startTick;
    mEndTick       = startTick + durationOf(phase);
    mLastCountdown = 0;
    mSwitchCued    = false;

    if (mPaused) {
        mPausedRemaining = durationOf(phase);
    } else if (phase == Phase::WORK) {
        mWorkRunStart = startTick;
    }

    if (withCue && phase == Phase::WORK) {
        cue(Cue::GO);
    }
}

void Engine::advance(uint32_t startTick, bool natural)
{
    switch (mPhase) {
        case Phase::GET_READY:
            enter(Phase::WORK, 0, 1, startTick, true);
            break;

        case Phase::WORK: {
            if (natural) {
                ++mExercisesDone;
            }
            const bool lastExercise = (mExercise + 1 >= kExerciseCount);
            if (lastExercise && mCircuit >= mCircuits) {
                // Workout over.
                if (!mPaused) {
                    bankActive(startTick);
                }
                mPaused    = false;
                // Counts as completed only if it was really done: the last
                // interval ran out and at least 75% of all intervals did.
                mCompleted = natural &&
                             mExercisesDone * 4u >= static_cast<uint32_t>(mCircuits) * kExerciseCount * 3u;
                mPhase     = Phase::DONE;
                cue(Cue::FINISHED);
                break;
            }
            const uint8_t nextEx = lastExercise ? 0 : static_cast<uint8_t>(mExercise + 1);
            const uint8_t nextC  = lastExercise ? static_cast<uint8_t>(mCircuit + 1) : mCircuit;
            enter(Phase::REST, nextEx, nextC, startTick, false);
            if (lastExercise) {
                cue(Cue::CIRCUIT_DONE, static_cast<uint8_t>(nextC - 1));
            } else {
                cue(Cue::REST);
            }
        } break;

        case Phase::REST:
            enter(Phase::WORK, mExercise, mCircuit, startTick, true);
            break;

        default:
            break;
    }
}

void Engine::bankWork(uint32_t now)
{
    mWorkBase += now - mWorkRunStart;
    mWorkRunStart = now;
}

void Engine::bankActive(uint32_t now)
{
    mActiveBase += now - mRunStart;
    mRunStart = now;
}

void Engine::cue(Cue c, uint8_t arg)
{
    if (mListener) {
        mListener->onCue(c, arg);
    }
}

void Engine::changed()
{
    if (mListener) {
        mListener->onStateChanged();
    }
}

} // namespace Workout
