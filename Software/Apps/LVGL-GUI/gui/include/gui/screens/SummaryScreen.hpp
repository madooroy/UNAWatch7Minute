/**
 ******************************************************************************
 * @file    SummaryScreen.hpp
 * @brief   Result of the workout: time, exercises, heart rate, streak count.
 *          R1 starts another workout with the same settings, R2 goes home.
 ******************************************************************************
 */

#ifndef SUMMARY_SCREEN_HPP
#define SUMMARY_SCREEN_HPP

#include <memory>

#include "SDK/GUI/LVGL/TimerRing.hpp"

#include "gui/screens/Screen.hpp"

class SummaryScreen : public Screen
{
public:
    explicit SummaryScreen(Model& model);
    ~SummaryScreen() override;

    void refresh() override;

protected:
    void onClick(uint8_t code) override;

private:
    std::unique_ptr<SDK::LVGL::TimerRing> mRing;
    lv_obj_t* mTitleLabel  = nullptr;
    lv_obj_t* mTime        = nullptr;
    lv_obj_t* mTimeCaption = nullptr;
    lv_obj_t* mRowLabel[3] = {};
    lv_obj_t* mRowValue[3] = {};
    lv_obj_t* mFooter      = nullptr;
};

#endif // SUMMARY_SCREEN_HPP
