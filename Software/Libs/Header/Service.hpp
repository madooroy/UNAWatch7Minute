/**
 ******************************************************************************
 * @file    Service.hpp
 * @brief   Background process of the 7-Minute app: owns the workout clock,
 *          plays every cue (buzzer, vibration, backlight), reads heart rate
 *          and persists settings.
 *
 * Keeping the clock and the cues in the service means the workout keeps
 * running and keeps beeping/buzzing on schedule even while the screen is
 * off or the GUI is not drawing: the user never has to look at the watch
 * mid-exercise.
 ******************************************************************************
 */

#ifndef SERVICE_HPP
#define SERVICE_HPP

#include <cstdint>

#include "SDK/Kernel/Kernel.hpp"
#include "SDK/SensorLayer/SensorConnection.hpp"
#include "SDK/SensorLayer/SensorDataBatch.hpp"

#include "Commands.hpp"
#include "WorkoutEngine.hpp"

class Service : public Workout::Engine::Listener
{
public:
    explicit Service(SDK::Kernel& kernel);
    ~Service() override;

    void run();

private:
    SDK::Kernel&            mKernel;
    bool                    mGuiStarted = false;
    Workout::Engine         mEngine;
    Workout::Phase          mLastPhase = Workout::Phase::IDLE;

    // Settings (persisted)
    uint8_t                 mCircuits     = 1;
    Workout::AlertMode      mAlertMode    = Workout::AlertMode::BEEP_AND_VIBRO;
    uint16_t                mWorkoutsDone = 0;

    // Heart rate (while a workout runs)
    SDK::Sensor::Connection mSensorHR;
    bool                    mHrConnected = false;
    uint8_t                 mHr          = 0;
    uint32_t                mHrSum       = 0;
    uint32_t                mHrCount     = 0;
    uint8_t                 mHrMax       = 0;

    // Last summary, re-sent if the GUI (re)starts while it is on display.
    CustomMessage::SummaryData mSummary;

    // -- Lifecycle ------------------------------------------------------------
    void onStartGUI();
    void onStopGUI();
    void shutdown();

    // -- GUI commands ---------------------------------------------------------
    void handleStart(const CustomMessage::WorkoutStart& msg);
    void handleControl(const CustomMessage::WorkoutControl& msg);
    void handleSettingsSave(const CustomMessage::SettingsSave& msg);

    // -- Engine::Listener -----------------------------------------------------
    void onCue(Workout::Cue cue, uint8_t arg) override;
    void onStateChanged() override;

    // -- Helpers --------------------------------------------------------------
    void sendState();
    void sendSettings();
    void buildSummary();
    void hrStart();
    void hrStop();
    void onSensorData(uint16_t handle, SDK::Sensor::DataBatch& data);

    void backlight(uint32_t onMs);
    void stopOutputs();

    void loadSettings();
    void saveSettings();
};

#endif // SERVICE_HPP
