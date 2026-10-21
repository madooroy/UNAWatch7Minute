/**
 ******************************************************************************
 * @file    HomeScreen.cpp
 ******************************************************************************
 */

#include "gui/screens/HomeScreen.hpp"

#include <cstdio>

#include "SDK/GUI/Button.hpp"
#include "SDK/GUI/LVGL/Draw.hpp"

#include "gui/Assets.hpp"
#include "gui/Theme.hpp"

namespace Draw = SDK::LVGL::Draw;
using SDK::LVGL::Buttons;

HomeScreen::HomeScreen(Model& model)
    : Screen(model)
{
    mTitle = std::make_unique<SDK::LVGL::Title>(mRoot, &poppins_italic_18, "7-MINUTE");

    // L1 zone (top left): rounds.
    mRoundsCaption = Draw::label(mRoot, &poppins_regular_15, "ROUNDS", 36, 46, 80, LV_TEXT_ALIGN_LEFT, Theme::kTextDim);
    mRoundsValue   = Draw::label(mRoot, &poppins_medium_18, "1", 36, 62, 80, LV_TEXT_ALIGN_LEFT, Theme::kText);

    // R1 zone (top right): start.
    mStart = Draw::label(mRoot, &poppins_medium_18, "START", 126, 54, 82, LV_TEXT_ALIGN_RIGHT, Theme::kAccent);

    // Centre: planned length and the protocol in one line.
    mDuration = Draw::label(mRoot, &poppins_semibold_48, "8:00", 40, 92, 160);
    mProtocol = Draw::label(mRoot, &poppins_regular_15, "12 moves  30s / 10s", 30, 136, 180, LV_TEXT_ALIGN_CENTER, Theme::kTextDim);

    // L2 zone (bottom left): alert style.
    mAlertsCaption = Draw::label(mRoot, &poppins_regular_15, "ALERTS", 44, 160, 110, LV_TEXT_ALIGN_LEFT, Theme::kTextDim);
    mAlertsValue   = Draw::label(mRoot, &poppins_medium_18, "", 44, 176, 130, LV_TEXT_ALIGN_LEFT, Theme::kText);

    // Lifetime count, bottom right next to the exit button's quiet zone.
    mDone = Draw::label(mRoot, &poppins_regular_15, "", 130, 160, 74, LV_TEXT_ALIGN_RIGHT, Theme::kWork);

    mButtons = std::make_unique<Buttons>(mRoot);
    mButtons->set(Buttons::WHITE, Buttons::WHITE, Buttons::AMBER, Buttons::WHITE);

    refresh();
}

HomeScreen::~HomeScreen()
{
    mTitle.reset();
}

void HomeScreen::refresh()
{
    char buf[24];
    const uint8_t rounds = mModel.settingCircuits();

    std::snprintf(buf, sizeof(buf), "%u", rounds);
    lv_label_set_text(mRoundsValue, buf);

    Theme::formatMinSec(buf, sizeof(buf), Workout::plannedDurationMs(rounds));
    lv_label_set_text(mDuration, buf);

    lv_label_set_text(mAlertsValue, Workout::alertModeName(mModel.settingAlertMode()));

    const uint16_t done = mModel.workoutsDone();
    if (done > 0) {
        std::snprintf(buf, sizeof(buf), "%u done", done);
        lv_label_set_text(mDone, buf);
        Draw::setHidden(mDone, false);
    } else {
        Draw::setHidden(mDone, true);
    }
}

void HomeScreen::onClick(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;
    switch (code) {
        case Btn::L1: {
            uint8_t r = static_cast<uint8_t>(mModel.settingCircuits() + 1);
            if (r > Workout::kMaxCircuits) {
                r = Workout::kMinCircuits;
            }
            mModel.setCircuits(r);
            refresh();
        } break;

        case Btn::L2: {
            uint8_t m = static_cast<uint8_t>(static_cast<uint8_t>(mModel.settingAlertMode()) + 1);
            if (m >= static_cast<uint8_t>(Workout::AlertMode::COUNT)) {
                m = 0;
            }
            mModel.setAlertMode(static_cast<Workout::AlertMode>(m));
            refresh();
        } break;

        case Btn::R1:
            mModel.startWorkout();
            break;

        case Btn::R2:
            mModel.exitApp();
            break;

        default:
            break;
    }
}
