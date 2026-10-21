/**
 ******************************************************************************
 * @file    Model.cpp
 * @brief   GUI-side mirror of the workout and channel to the service.
 ******************************************************************************
 */

#include "gui/model/Model.hpp"
#include "gui/model/ModelListener.hpp"

#include "SDK/Kernel/KernelProviderGUI.hpp"
#include "SDK/Messages/MessageGuard.hpp"
#include "SDK/Port/LVGL/LvglPort.hpp"

#define LOG_MODULE_PRX      "Model"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

Model::Model()
    : mKernel(SDK::KernelProviderGUI::GetInstance().getKernel())
{
    SDK::LVGL::Port::GetInstance().setAppLifeCycleCallback(this);
    SDK::LVGL::Port::GetInstance().setCustomMessageHandler(this);

#if defined(SIMULATOR)
    LOG_INFO("7-Minute simulator. Keys: 1=L1  2=L2  3=R1  4=R2, Esc = close\n");
#endif
}

// -- Commands -----------------------------------------------------------------

void Model::startWorkout()
{
    SDK::send_msg<CustomMessage::WorkoutStart>(mKernel, mCircuits, mAlertMode);
}

void Model::control(CustomMessage::Control cmd)
{
    SDK::send_msg<CustomMessage::WorkoutControl>(mKernel, cmd);
}

void Model::setCircuits(uint8_t circuits)
{
    mCircuits = circuits;
    SDK::send_msg<CustomMessage::SettingsSave>(mKernel, mCircuits, mAlertMode);
}

void Model::setAlertMode(Workout::AlertMode mode)
{
    mAlertMode = mode;
    SDK::send_msg<CustomMessage::SettingsSave>(mKernel, mCircuits, mAlertMode);
}

void Model::exitApp()
{
    LOG_INFO("Exit\n");
    SDK::LVGL::Port::GetInstance().setAppLifeCycleCallback(nullptr);
    SDK::LVGL::Port::GetInstance().setCustomMessageHandler(nullptr);
    mKernel.sys.exit();
}

// -- State --------------------------------------------------------------------

uint32_t Model::remainingMs() const
{
    if (mState.paused) {
        return mState.remainingMs;
    }
    const int32_t r = static_cast<int32_t>(mState.endTick - now());
    return r > 0 ? static_cast<uint32_t>(r) : 0;
}

// -- Lifecycle ----------------------------------------------------------------

void Model::onStart()   {}
void Model::onResume()  {}
void Model::onSuspend() {}
void Model::onStop()    {}

void Model::onFrame()
{
    if (mListener) {
        mListener->onFrame();
    }
}

// -- Service messages ---------------------------------------------------------

bool Model::customMessageHandler(SDK::MessageBase* msg)
{
    switch (msg->getType()) {
        case CustomMessage::WO_STATE: {
            auto* m                = static_cast<CustomMessage::WorkoutState*>(msg);
            mState.phase           = m->phase;
            mState.paused          = m->paused;
            mState.exercise        = m->exercise;
            mState.circuit         = m->circuit;
            mState.circuits        = m->circuits;
            mState.phaseDurationMs = m->phaseDurationMs;
            mState.endTick         = m->endTick;
            mState.remainingMs     = m->remainingMs;
            mHaveState             = true;
            if (mListener) {
                mListener->onStateChanged();
            }
        } break;

        case CustomMessage::WO_SETTINGS: {
            auto* m       = static_cast<CustomMessage::Settings*>(msg);
            mCircuits     = m->circuits;
            mAlertMode    = static_cast<Workout::AlertMode>(m->alertMode);
            mWorkoutsDone = m->workoutsDone;
            if (mListener) {
                mListener->onSettingsChanged();
            }
        } break;

        case CustomMessage::WO_HR: {
            mHr = static_cast<CustomMessage::HeartRate*>(msg)->bpm;
            if (mListener) {
                mListener->onHeartRate();
            }
        } break;

        case CustomMessage::WO_SUMMARY: {
            mSummary      = static_cast<CustomMessage::Summary*>(msg)->data;
            mWorkoutsDone = mSummary.workoutsDone;
            if (mListener) {
                mListener->onSummary();
            }
        } break;

        default:
            break;
    }
    return true;
}
