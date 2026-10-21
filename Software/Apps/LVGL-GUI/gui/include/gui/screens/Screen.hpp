/**
 ******************************************************************************
 * @file    Screen.hpp
 * @brief   Base of the app's three screens (Home, Workout, Summary).
 *
 * Each screen owns one LVGL screen object. The kernel delivers button events
 * to the active LVGL screen as LV_EVENT_KEY; the base filters out the press /
 * release codes and hands clicks to onClick().
 ******************************************************************************
 */

#ifndef SCREEN_HPP
#define SCREEN_HPP

#include <cstdint>
#include <memory>

#include "lvgl.h"

#include "SDK/GUI/LVGL/Buttons.hpp"

#include "gui/model/Model.hpp"

class Screen
{
public:
    explicit Screen(Model& model);
    virtual ~Screen();

    Screen(const Screen&)            = delete;
    Screen& operator=(const Screen&) = delete;

    lv_obj_t* root() const { return mRoot; }

    /// Called when the screen becomes active, and whenever the model changes.
    virtual void refresh() = 0;
    /// Called every display frame while active.
    virtual void onFrame() {}

protected:
    /// A button click (SDK::GUI::Button::L1 / L2 / R1 / R2).
    virtual void onClick(uint8_t code) = 0;

    Model&    mModel;
    lv_obj_t* mRoot = nullptr;
    std::unique_ptr<SDK::LVGL::Buttons> mButtons;

private:
    static void keyEventCb(lv_event_t* e);
};

#endif // SCREEN_HPP
