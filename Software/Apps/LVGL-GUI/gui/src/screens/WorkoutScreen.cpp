/**
 ******************************************************************************
 * @file    WorkoutScreen.cpp
 ******************************************************************************
 */

#include "gui/screens/WorkoutScreen.hpp"

#include <cmath>
#include <cstdio>

#include "SDK/GUI/Button.hpp"
#include "SDK/GUI/LVGL/Draw.hpp"

#include "gui/Assets.hpp"
#include "gui/Theme.hpp"

namespace Draw = SDK::LVGL::Draw;
using SDK::LVGL::Buttons;
using Workout::Phase;
using CustomMessage::Control;

namespace
{
// Progress dots: an arc across the top of the face, inside the ring.
constexpr float   kDotRadiusPath = 99.0f;
constexpr float   kDotStartDeg   = -33.0f;   // TouchGFX angles, 0 = 12 o'clock
constexpr float   kDotStepDeg    = 6.0f;
constexpr int32_t kDotSize       = 3;
constexpr float   kPi            = 3.14159265f;
} // namespace

WorkoutScreen::WorkoutScreen(Model& model)
    : Screen(model)
{
    mRing = std::make_unique<SDK::LVGL::TimerRing>(mRoot);

    for (uint8_t i = 0; i < Workout::kExerciseCount; ++i) {
        const float a  = (kDotStartDeg + kDotStepDeg * i) * kPi / 180.0f;
        const int32_t x = static_cast<int32_t>(lroundf(120.0f + kDotRadiusPath * sinf(a)));
        const int32_t y = static_cast<int32_t>(lroundf(120.0f - kDotRadiusPath * cosf(a)));
        mDots[i] = Draw::dot(mRoot, x, y, kDotSize, Theme::kDotTodo);
    }

    mPhase   = Draw::label(mRoot, &poppins_medium_18, "", 40, 36, 160, LV_TEXT_ALIGN_CENTER, Theme::kReady);
    mSeconds = Draw::label(mRoot, &poppins_semibold_96, "", 20, 60, 200, LV_TEXT_ALIGN_CENTER, Theme::kText);
    mName    = Draw::label(mRoot, &poppins_semibold_26, "", 16, 136, 208, LV_TEXT_ALIGN_CENTER, Theme::kText);
    mDetail  = Draw::label(mRoot, &poppins_regular_15, "", 30, 168, 180, LV_TEXT_ALIGN_CENTER, Theme::kTextDim);

    // Bottom line: "3/12" to the left of centre, heart + bpm to the right.
    mCounter  = Draw::label(mRoot, &poppins_medium_18, "", 50, 188, 64, LV_TEXT_ALIGN_RIGHT, Theme::kText);
    mHeartDot = Draw::dot(mRoot, 128, 199, 4, Theme::kHeart);
    mHeart    = Draw::label(mRoot, &poppins_medium_18, "--", 136, 188, 60, LV_TEXT_ALIGN_LEFT, Theme::kText);

    mButtons = std::make_unique<Buttons>(mRoot);

    refresh();
}

WorkoutScreen::~WorkoutScreen()
{
    mRing.reset();
}

uint32_t WorkoutScreen::phaseColor() const
{
    if (mModel.paused()) {
        return Theme::kPaused;
    }
    switch (mModel.phase()) {
        case Phase::GET_READY: return Theme::kReady;
        case Phase::WORK:      return Theme::kWork;
        case Phase::REST:      return Theme::kRest;
        default:               return Theme::kText;
    }
}

void WorkoutScreen::refresh()
{
    // The prompt survives until the pause it requested has been confirmed;
    // a resume from anywhere else dismisses it.
    if (mModel.paused()) {
        mAwaitPause = false;
    } else if (!mAwaitPause) {
        mConfirmEnd = false;
    }

    const uint8_t           ex   = mModel.exercise() < Workout::kExerciseCount ? mModel.exercise() : 0;
    const Workout::Exercise& e   = Workout::kExercises[ex];
    const Phase             ph   = mModel.phase();
    const uint32_t          col  = phaseColor();
    char                    buf[24];

    // Progress dots: done in their muscle-group colour, current/next white.
    for (uint8_t i = 0; i < Workout::kExerciseCount; ++i) {
        uint32_t c = Theme::kDotTodo;
        if (i < ex) {
            c = Theme::categoryColor(Workout::kExercises[i].category);
        } else if (i == ex && ph != Phase::GET_READY) {
            c = Theme::kText;
        }
        lv_obj_set_style_bg_color(mDots[i], Draw::rgb(c), LV_PART_MAIN);
    }

    mRing->setColor(col);
    lv_obj_set_style_text_color(mSeconds, Draw::rgb(mModel.paused() ? Theme::kPaused : Theme::kText), LV_PART_MAIN);

    // Long names drop to a smaller face rather than touch the ring.
    lv_point_t size;
    lv_text_get_size(&size, e.name, &poppins_semibold_26, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const bool longName = size.x > 176;
    lv_obj_set_style_text_font(mName, longName ? &poppins_semibold_21 : &poppins_semibold_26, LV_PART_MAIN);
    lv_obj_set_y(mName, longName ? 140 : 136);
    lv_label_set_text(mName, e.name);
    lv_obj_set_style_text_color(mName, Draw::rgb(ph == Phase::WORK ? Theme::kText : col), LV_PART_MAIN);

    if (mConfirmEnd) {
        lv_label_set_text(mDetail, "R2 end  /  R1 back");
        lv_obj_set_style_text_color(mDetail, Draw::rgb(Theme::kTextDim), LV_PART_MAIN);
    } else if (mModel.paused()) {
        lv_label_set_text(mDetail, "R1 resume  /  R2 end");
        lv_obj_set_style_text_color(mDetail, Draw::rgb(Theme::kTextDim), LV_PART_MAIN);
    } else if (ph == Phase::WORK) {
        lv_label_set_text(mDetail, Workout::categoryName(e.category));
        lv_obj_set_style_text_color(mDetail, Draw::rgb(Theme::categoryColor(e.category)), LV_PART_MAIN);
    } else {
        std::snprintf(buf, sizeof(buf), "Next: %s", e.tip);
        lv_label_set_text(mDetail, buf);
        lv_obj_set_style_text_color(mDetail, Draw::rgb(Theme::kTextDim), LV_PART_MAIN);
    }

    std::snprintf(buf, sizeof(buf), "%u/%u", ex + 1, Workout::kExerciseCount);
    lv_label_set_text(mCounter, buf);

    // Button hints follow the UNA colour language: amber act, green confirm,
    // red discard, white back.
    if (mConfirmEnd) {
        mButtons->set(Buttons::NONE, Buttons::NONE, Buttons::WHITE, Buttons::RED);
    } else if (mModel.paused()) {
        mButtons->set(Buttons::NONE, Buttons::NONE, Buttons::GREEN, Buttons::RED);
    } else {
        mButtons->set(Buttons::NONE, Buttons::NONE, Buttons::AMBER, Buttons::WHITE);
    }

    refreshHeartRate();
    mShownSide = -1;
    updatePhaseLabel();
    updateClock(true);
}

void WorkoutScreen::refreshHeartRate()
{
    const uint8_t hr = mModel.heartRate();
    char          buf[8];
    if (hr > 0) {
        std::snprintf(buf, sizeof(buf), "%u", hr);
        lv_label_set_text(mHeart, buf);
        lv_obj_set_style_bg_color(mHeartDot, Draw::rgb(Theme::kHeart), LV_PART_MAIN);
    } else {
        lv_label_set_text(mHeart, "--");
        lv_obj_set_style_bg_color(mHeartDot, Draw::rgb(Theme::kTrack), LV_PART_MAIN);
    }
}

void WorkoutScreen::updatePhaseLabel()
{
    const Phase   ph  = mModel.phase();
    const uint8_t ex  = mModel.exercise() < Workout::kExerciseCount ? mModel.exercise() : 0;
    char          buf[24];
    const char*   txt = "";
    uint32_t      col = phaseColor();

    if (mConfirmEnd) {
        txt = "END WORKOUT?";
        col = Theme::kDanger;
    } else if (mModel.paused()) {
        txt = "PAUSED";
        col = Theme::kText;
    } else if (ph == Phase::GET_READY) {
        txt = "GET READY";
    } else if (ph == Phase::WORK) {
        if (Workout::kExercises[ex].switchSides) {
            const bool secondHalf = mModel.remainingMs() <= Workout::kWorkMs / 2;
            mShownSide = secondHalf ? 1 : 0;
            txt = secondHalf ? "LEFT SIDE" : "RIGHT SIDE";
        } else if (mModel.circuits() > 1) {
            std::snprintf(buf, sizeof(buf), "ROUND %u/%u", mModel.circuit(), mModel.circuits());
            txt = buf;
        } else {
            txt = "GO!";
        }
    } else if (ph == Phase::REST) {
        if (ex == 0 && mModel.circuit() > 1) {
            std::snprintf(buf, sizeof(buf), "ROUND %u/%u", mModel.circuit(), mModel.circuits());
            txt = buf;
        } else {
            txt = "REST";
        }
    }
    lv_label_set_text(mPhase, txt);
    lv_obj_set_style_text_color(mPhase, Draw::rgb(col), LV_PART_MAIN);
}

void WorkoutScreen::onFrame()
{
    updateClock(false);

    // Side plank: flip the side label at half-time without waiting for a snapshot.
    const uint8_t ex = mModel.exercise() < Workout::kExerciseCount ? mModel.exercise() : 0;
    if (mModel.phase() == Phase::WORK && !mModel.paused() && !mConfirmEnd &&
        Workout::kExercises[ex].switchSides) {
        const int8_t side = mModel.remainingMs() <= Workout::kWorkMs / 2 ? 1 : 0;
        if (side != mShownSide) {
            updatePhaseLabel();
        }
    }
}

void WorkoutScreen::updateClock(bool force)
{
    const uint32_t remaining = mModel.remainingMs();
    const uint32_t duration  = mModel.phaseDurationMs();

    const int32_t secs = static_cast<int32_t>((remaining + 999) / 1000);
    if (force || secs != mShownSeconds) {
        mShownSeconds = secs;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%ld", static_cast<long>(secs));
        lv_label_set_text(mSeconds, buf);
    }

    const uint32_t permille = duration ? (remaining * 1000u + duration / 2) / duration : 0;
    if (force || permille != mShownRing) {
        mShownRing = permille;
        mRing->setRemaining(permille);
    }
}

void WorkoutScreen::onClick(uint8_t code)
{
    namespace Btn = SDK::GUI::Button;

    if (mConfirmEnd) {
        if (code == Btn::R2) {
            mModel.control(Control::FINISH);
        } else {
            mConfirmEnd = false;
            mAwaitPause = false;
            mModel.control(Control::RESUME);
            refresh();
        }
        return;
    }

    switch (code) {
        case Btn::R1:
            mModel.control(Control::TOGGLE_PAUSE);
            break;
        case Btn::R2:
            // Never end a workout on one stray press: pause and ask.
            mConfirmEnd = true;
            if (!mModel.paused()) {
                mAwaitPause = true;
                mModel.control(Control::PAUSE);
            }
            refresh();
            break;
        case Btn::L1:
            mModel.control(Control::PREV);
            break;
        case Btn::L2:
            mModel.control(Control::NEXT);
            break;
        default:
            break;
    }
}
