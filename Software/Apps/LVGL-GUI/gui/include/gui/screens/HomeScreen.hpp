/**
 ******************************************************************************
 * @file    HomeScreen.hpp
 * @brief   Start screen. Every control sits next to its button, so there is
 *          no menu to navigate:
 *
 *            L1  rounds 1 / 2 / 3          R1  START
 *            L2  alerts beep+buzz / buzz / beep   R2  exit
 ******************************************************************************
 */

#ifndef HOME_SCREEN_HPP
#define HOME_SCREEN_HPP

#include <memory>

#include "SDK/GUI/LVGL/Title.hpp"

#include "gui/screens/Screen.hpp"

class HomeScreen : public Screen
{
public:
    explicit HomeScreen(Model& model);
    ~HomeScreen() override;

    void refresh() override;

protected:
    void onClick(uint8_t code) override;

private:
    std::unique_ptr<SDK::LVGL::Title> mTitle;
    lv_obj_t* mRoundsCaption = nullptr;
    lv_obj_t* mRoundsValue   = nullptr;
    lv_obj_t* mStart         = nullptr;
    lv_obj_t* mDuration      = nullptr;
    lv_obj_t* mProtocol      = nullptr;
    lv_obj_t* mDone          = nullptr;
    lv_obj_t* mAlertsCaption = nullptr;
    lv_obj_t* mAlertsValue   = nullptr;
};

#endif // HOME_SCREEN_HPP
