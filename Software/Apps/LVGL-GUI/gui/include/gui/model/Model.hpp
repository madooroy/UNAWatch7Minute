/**
 ******************************************************************************
 * @file    Model.hpp
 * @brief   GUI-side mirror of the service's workout state, and the GUI's
 *          channel back to the service.
 *
 * The service is the single source of truth: the model only stores the last
 * snapshot it received and turns user intent into commands. The countdown is
 * extrapolated from the snapshot's end tick and the shared kernel clock.
 ******************************************************************************
 */

#ifndef MODEL_HPP
#define MODEL_HPP

#include <cstdint>

#include "SDK/Interfaces/ICustomMessageHandler.hpp"
#include "SDK/Interfaces/IGuiLifeCycleCallback.hpp"
#include "SDK/Kernel/Kernel.hpp"

#include "Commands.hpp"
#include "WorkoutEngine.hpp"

class ModelListener;

class Model : public SDK::Interface::IGuiLifeCycleCallback,
              public SDK::Interface::ICustomMessageHandler
{
public:
    Model();

    void bind(ModelListener* listener) { mListener = listener; }

    // -- Commands to the service ------------------------------------------------
    void startWorkout();
    void control(CustomMessage::Control cmd);
    void setCircuits(uint8_t circuits);
    void setAlertMode(Workout::AlertMode mode);
    void exitApp();

    // -- Mirrored state -----------------------------------------------------------
    Workout::Phase     phase() const      { return static_cast<Workout::Phase>(mState.phase); }
    bool               paused() const     { return mState.paused != 0; }
    uint8_t            exercise() const   { return mState.exercise; }
    uint8_t            circuit() const    { return mState.circuit; }
    uint8_t            circuits() const   { return mState.circuits; }
    uint32_t           phaseDurationMs() const { return mState.phaseDurationMs; }
    uint32_t           remainingMs() const;
    bool               haveState() const  { return mHaveState; }

    uint8_t            settingCircuits() const  { return mCircuits; }
    Workout::AlertMode settingAlertMode() const { return mAlertMode; }
    uint16_t           workoutsDone() const     { return mWorkoutsDone; }
    uint8_t            heartRate() const        { return mHr; }
    const CustomMessage::SummaryData& summary() const { return mSummary; }

    uint32_t           now() const { return mKernel.sys.getTimeMs(); }

protected:
    ModelListener*     mListener = nullptr;
    const SDK::Kernel& mKernel;

    struct StateData {
        uint8_t  phase           = 0;
        uint8_t  paused          = 0;
        uint8_t  exercise        = 0;
        uint8_t  circuit         = 1;
        uint8_t  circuits        = 1;
        uint32_t phaseDurationMs = 0;
        uint32_t endTick         = 0;
        uint32_t remainingMs     = 0;
    } mState;
    bool               mHaveState    = false;

    uint8_t            mCircuits     = 1;
    Workout::AlertMode mAlertMode    = Workout::AlertMode::BEEP_AND_VIBRO;
    uint16_t           mWorkoutsDone = 0;
    uint8_t            mHr           = 0;
    CustomMessage::SummaryData mSummary;

    // IGuiLifeCycleCallback
    void onStart() override;
    void onFrame() override;
    void onResume() override;
    void onSuspend() override;
    void onStop() override;

    // ICustomMessageHandler
    bool customMessageHandler(SDK::MessageBase* msg) override;
};

#endif // MODEL_HPP
