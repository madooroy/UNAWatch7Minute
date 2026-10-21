/**
 ******************************************************************************
 * @file    ModelListener.hpp
 * @brief   Events the Model raises towards the UI.
 ******************************************************************************
 */

#ifndef MODEL_LISTENER_HPP
#define MODEL_LISTENER_HPP

class ModelListener
{
public:
    virtual ~ModelListener() = default;

    virtual void onFrame() {}           ///< Once per display frame (10 Hz)
    virtual void onStateChanged() {}    ///< New workout snapshot
    virtual void onSettingsChanged() {} ///< Settings / lifetime count arrived
    virtual void onHeartRate() {}       ///< New heart-rate reading
    virtual void onSummary() {}         ///< A workout ended
};

#endif // MODEL_LISTENER_HPP
