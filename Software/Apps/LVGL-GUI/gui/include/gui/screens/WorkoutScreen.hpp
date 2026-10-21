/**
 ******************************************************************************
 * @file    WorkoutScreen.hpp
 * @brief   The in-workout face, readable at a glance mid-exercise:
 *
 *              . . . o o o o o o o o o      <- 12 progress dots
 *                     GET READY / GO / REST / PAUSED
 *                          27               <- seconds left (huge)
 *                       Push-Ups            <- current (or next) exercise
 *                      UPPER BODY           <- muscle group / coaching tip
 *                     3/12   * 132          <- position, heart rate
 *
 *   A ring around the face drains with the interval, coloured by phase.
 *
 *   Buttons: R1 pause/resume, R2 end (asks first), L1 back, L2 skip.
 ******************************************************************************
 */

#ifndef WORKOUT_SCREEN_HPP
#define WORKOUT_SCREEN_HPP

#include <memory>

#include "SDK/GUI/LVGL/TimerRing.hpp"

#include "WorkoutPlan.hpp"
#include "gui/screens/Screen.hpp"

class WorkoutScreen : public Screen
{
public:
    explicit WorkoutScreen(Model& model);
    ~WorkoutScreen() override;

    void refresh() override;
    void onFrame() override;
    void refreshHeartRate();

protected:
    void onClick(uint8_t code) override;

private:
    std::unique_ptr<SDK::LVGL::TimerRing> mRing;
    lv_obj_t* mDots[Workout::kExerciseCount] = {};
    lv_obj_t* mPhase    = nullptr;
    lv_obj_t* mSeconds  = nullptr;
    lv_obj_t* mName     = nullptr;
    lv_obj_t* mDetail   = nullptr;
    lv_obj_t* mCounter  = nullptr;
    lv_obj_t* mHeartDot = nullptr;
    lv_obj_t* mHeart    = nullptr;

    bool     mConfirmEnd   = false;  ///< "End workout?" prompt showing
    bool     mAwaitPause   = false;  ///< Pause requested for the prompt, not yet confirmed
    int32_t  mShownSeconds = -1;
    uint32_t mShownRing    = 0xFFFFFFFF;
    int8_t   mShownSide    = -1;     ///< side-plank half shown

    uint32_t phaseColor() const;
    void     updateClock(bool force);
    void     updatePhaseLabel();
};

#endif // WORKOUT_SCREEN_HPP
