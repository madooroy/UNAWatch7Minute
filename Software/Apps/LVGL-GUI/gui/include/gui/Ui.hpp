/**
 ******************************************************************************
 * @file    Ui.hpp
 * @brief   Owns the three screens and shows the one that matches the
 *          service's workout phase (IDLE -> Home, running -> Workout,
 *          DONE -> Summary).
 ******************************************************************************
 */

#ifndef UI_HPP
#define UI_HPP

#include "gui/model/Model.hpp"
#include "gui/model/ModelListener.hpp"
#include "gui/screens/HomeScreen.hpp"
#include "gui/screens/SummaryScreen.hpp"
#include "gui/screens/WorkoutScreen.hpp"

class Ui : public ModelListener
{
public:
    explicit Ui(Model& model);

    void onFrame() override;
    void onStateChanged() override;
    void onSettingsChanged() override;
    void onHeartRate() override;
    void onSummary() override;

private:
    Model&        mModel;
    HomeScreen    mHome;
    WorkoutScreen mWorkout;
    SummaryScreen mSummary;
    Screen*       mActive = nullptr;

    void show(Screen& s);
    Screen& screenFor(Workout::Phase phase);
};

#endif // UI_HPP
