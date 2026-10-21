/**
 ******************************************************************************
 * @file    SummaryScreen.cpp
 ******************************************************************************
 */

#include "gui/screens/SummaryScreen.hpp"

#include <cstdio>

#include "SDK/GUI/Button.hpp"
#include "SDK/GUI/LVGL/Draw.hpp"

#include "gui/Assets.hpp"
#include "gui/Theme.hpp"

namespace Draw = SDK::LVGL::Draw;
using SDK::LVGL::Buttons;

namespace
{
// Rows narrow towards the bottom of the round face.
constexpr int32_t kRowY[3] = { 114, 138, 162 };
constexpr int32_t kRowX[3] = { 36, 40, 50 };
} // namespace

SummaryScreen::SummaryScreen(Model& model)
    : Screen(model)
{
    mRing  = std::make_unique<SDK::LVGL::TimerRing>(mRoot);
    // Title sits inside the ring (the SDK Title widget's position is under it).
    mTitleLabel = Draw::label(mRoot, &poppins_italic_18, "COMPLETE", 50, 28, 140, LV_TEXT_ALIGN_CENTER, Theme::kWork);

    mTime        = Draw::label(mRoot, &poppins_semibold_48, "0:00", 40, 46, 160);
    mTimeCaption = Draw::label(mRoot, &poppins_regular_15, "WORKOUT TIME", 40, 92, 160, LV_TEXT_ALIGN_CENTER, Theme::kTextDim);

    for (int i = 0; i < 3; ++i) {
        const int32_t w = 240 - 2 * kRowX[i];
        mRowLabel[i] = Draw::label(mRoot, &poppins_regular_15, "", kRowX[i], kRowY[i] + 3, w, LV_TEXT_ALIGN_LEFT, Theme::kTextDim);
        mRowValue[i] = Draw::label(mRoot, &poppins_medium_18, "", kRowX[i], kRowY[i], w, LV_TEXT_ALIGN_RIGHT, Theme::kText);
    }

    mFooter = Draw::label(mRoot, &poppins_regular_15, "", 50, 188, 140, LV_TEXT_ALIGN_CENTER, Theme::kWork);

    mButtons = std::make_unique<Buttons>(mRoot);
    mButtons->set(Buttons::NONE, Buttons::NONE, Buttons::AMBER, Buttons::WHITE);

    refresh();
}

SummaryScreen::~SummaryScreen()
{
    mRing.reset();
}

void SummaryScreen::refresh()
{
    const CustomMessage::SummaryData& s = mModel.summary();
    char buf[24];

    lv_label_set_text(mTitleLabel, s.completed ? "COMPLETE" : "ENDED EARLY");
    lv_obj_set_style_text_color(mTitleLabel, Draw::rgb(s.completed ? Theme::kWork : Theme::kReady), LV_PART_MAIN);

    // Ring shows how much of the plan was done.
    const uint32_t permille = s.exercisesPlanned ? (s.exercisesDone * 1000u) / s.exercisesPlanned : 0;
    mRing->setColor(s.completed ? Theme::kWork : Theme::kReady);
    mRing->setProgress(s.completed ? 1000 : permille);

    Theme::formatMinSec(buf, sizeof(buf), s.activeMs);
    lv_label_set_text(mTime, buf);

    lv_label_set_text(mRowLabel[0], "Exercises");
    std::snprintf(buf, sizeof(buf), "%u/%u", s.exercisesDone, s.exercisesPlanned);
    lv_label_set_text(mRowValue[0], buf);

    lv_label_set_text(mRowLabel[1], "Avg HR");
    lv_label_set_text(mRowLabel[2], "Max HR");
    if (s.avgHr > 0) {
        std::snprintf(buf, sizeof(buf), "%u", s.avgHr);
        lv_label_set_text(mRowValue[1], buf);
        std::snprintf(buf, sizeof(buf), "%u", s.maxHr);
        lv_label_set_text(mRowValue[2], buf);
    } else {
        lv_label_set_text(mRowValue[1], "--");
        lv_label_set_text(mRowValue[2], "--");
    }

    if (s.completed) {
        std::snprintf(buf, sizeof(buf), "Workout #%u", s.workoutsDone);
        lv_label_set_text(mFooter, buf);
        Draw::setHidden(mFooter, false);
    } else {
        Draw::setHidden(mFooter, true);
    }
}

void SummaryScreen::onClick(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    if (code == Btn::R1) {
        mModel.startWorkout();           // again, same settings
    } else if (code == Btn::R2) {
        mModel.control(CustomMessage::Control::RESET);
    }
}
