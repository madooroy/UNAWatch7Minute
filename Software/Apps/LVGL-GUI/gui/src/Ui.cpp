/**
 ******************************************************************************
 * @file    Ui.cpp
 ******************************************************************************
 */

#include "gui/Ui.hpp"

using Workout::Phase;

Ui::Ui(Model& model)
    : mModel(model)
    , mHome(model)
    , mWorkout(model)
    , mSummary(model)
{
    mModel.bind(this);
    show(mHome);
}

Screen& Ui::screenFor(Phase phase)
{
    switch (phase) {
        case Phase::GET_READY:
        case Phase::WORK:
        case Phase::REST:
            return mWorkout;
        case Phase::DONE:
            return mSummary;
        default:
            return mHome;
    }
}

void Ui::show(Screen& s)
{
    s.refresh();
    if (mActive != &s) {
        mActive = &s;
        lv_screen_load(s.root());
    }
}

void Ui::onFrame()
{
    if (mActive) {
        mActive->onFrame();
    }
}

void Ui::onStateChanged()
{
    show(screenFor(mModel.phase()));
}

void Ui::onSettingsChanged()
{
    mHome.refresh();
}

void Ui::onHeartRate()
{
    mWorkout.refreshHeartRate();
}

void Ui::onSummary()
{
    mSummary.refresh();
    mHome.refresh();
}
