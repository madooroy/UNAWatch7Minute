/**
 ******************************************************************************
 * @file    Service.cpp
 * @brief   7-Minute service: workout clock, cues, heart rate, settings.
 ******************************************************************************
 */

#include "Service.hpp"

#include <cstdio>
#include <cstring>

#include "SDK/Messages/CommandMessages.hpp"
#include "SDK/Messages/MessageGuard.hpp"
#include "SDK/Messages/SensorLayerMessages.hpp"
#include "SDK/SensorLayer/DataParsers/SensorDataParserHeartRate.hpp"

#define LOG_MODULE_PRX      "Service"
#define LOG_MODULE_LEVEL    LOG_LEVEL_INFO
#include "SDK/UnaLogger/Logger.h"

using Workout::Cue;
using Workout::Phase;
using VibroFx = SDK::Message::RequestVibroPlay::Effect;

namespace
{
/// Stay alive this long after launch waiting for the GUI.
constexpr uint32_t kStartupGraceMs = 5000;
/// Idle poll period (nothing scheduled).
constexpr uint32_t kIdlePollMs = 1000;

constexpr const char kSettingsPath[] = "sevenminute.cfg";

// -- Cue sound/haptic design ---------------------------------------------------
//
// Each cue is distinct by ear AND by feel, so it can be told apart without
// looking: a ticking 3-2-1, a long high "go", a falling double "rest", a
// triple chirp to switch sides and a rising fanfare at the end.

struct Tone {
    uint16_t ms;
    uint16_t hz;      // 0 = rest (silence)
    uint8_t  volume;  // 0, 33, 66, 100
};

struct Buzz {
    uint8_t  effect;  // VibroFx, 0 = pause
    uint16_t pauseMs;
};

struct CuePattern {
    Tone    tones[SDK::Message::RequestBuzzerPlay::skMaxNotes];
    uint8_t toneCount;
    Buzz    buzz[SDK::Message::RequestVibroPlay::skMaxNotes];
    uint8_t buzzCount;
    uint16_t backlightMs;   // 0 = leave the backlight alone
};

// clang-format off
const CuePattern kCountdown = {
    { { 110, 2000, 66 } }, 1,
    { { VibroFx::SHARP_TICK_1_100, 0 } }, 1,
    4000,
};
const CuePattern kGo = {
    { { 650, 2800, 100 } }, 1,
    { { VibroFx::STRONG_BUZZ_100, 0 } }, 1,
    4000,
};
const CuePattern kRest = {
    { { 220, 2200, 100 }, { 90, 0, 0 }, { 320, 1500, 100 } }, 3,
    { { VibroFx::DOUBLE_CLICK_100, 0 } }, 1,
    4000,
};
const CuePattern kSwitch = {
    { { 120, 2400, 100 }, { 80, 0, 0 }, { 120, 2400, 100 }, { 80, 0, 0 }, { 120, 2400, 100 } }, 5,
    { { VibroFx::SHORT_DOUBLE_CLICK_STRONG_1_100, 0 }, { 0, 150 },
      { VibroFx::SHORT_DOUBLE_CLICK_STRONG_1_100, 0 } }, 3,
    4000,
};
const CuePattern kCircuitDone = {
    { { 160, 1600, 100 }, { 60, 0, 0 }, { 160, 2000, 100 }, { 60, 0, 0 }, { 300, 2400, 100 } }, 5,
    { { VibroFx::LONG_DOUBLE_SHARP_CLICK_STRONG_1_100, 0 }, { 0, 200 },
      { VibroFx::STRONG_BUZZ_100, 0 } }, 3,
    5000,
};
const CuePattern kFinished = {
    { { 150, 2093, 100 }, { 50, 0, 0 }, { 150, 2637, 100 }, { 50, 0, 0 },
      { 150, 3136, 100 }, { 50, 0, 0 }, { 500, 4186, 100 } }, 7,
    { { VibroFx::ALERT_750MS_100, 0 }, { 0, 250 }, { VibroFx::ALERT_750MS_100, 0 } }, 3,
    8000,
};
const CuePattern kPaused = {
    { { 90, 1200, 66 } }, 1,
    { { VibroFx::SOFT_BUMP_100, 0 } }, 1,
    0,
};
const CuePattern kResumed = {
    { { 90, 2000, 66 } }, 1,
    { { VibroFx::SHARP_CLICK_100, 0 } }, 1,
    0,
};
// clang-format on

const CuePattern* patternFor(Cue cue)
{
    switch (cue) {
        case Cue::COUNTDOWN:    return &kCountdown;
        case Cue::GO:           return &kGo;
        case Cue::REST:         return &kRest;
        case Cue::SWITCH_SIDES: return &kSwitch;
        case Cue::CIRCUIT_DONE: return &kCircuitDone;
        case Cue::FINISHED:     return &kFinished;
        case Cue::PAUSED:       return &kPaused;
        case Cue::RESUMED:      return &kResumed;
    }
    return nullptr;
}

const char* cueName(Cue cue)
{
    switch (cue) {
        case Cue::COUNTDOWN:    return "COUNTDOWN";
        case Cue::GO:           return "GO";
        case Cue::REST:         return "REST";
        case Cue::SWITCH_SIDES: return "SWITCH_SIDES";
        case Cue::CIRCUIT_DONE: return "CIRCUIT_DONE";
        case Cue::FINISHED:     return "FINISHED";
        case Cue::PAUSED:       return "PAUSED";
        case Cue::RESUMED:      return "RESUMED";
    }
    return "?";
}
} // namespace

// -----------------------------------------------------------------------------

Service::Service(SDK::Kernel& kernel)
    : mKernel(kernel)
    , mSensorHR(SDK::Sensor::Type::HEART_RATE, 1000, 2000)
{
    mEngine.setListener(this);
}

Service::~Service()
{
    mEngine.setListener(nullptr);
}

void Service::run()
{
    LOG_INFO("7-Minute service started\n");
    loadSettings();

    const uint32_t startTime = mKernel.sys.getTimeMs();

    while (true) {
        const uint32_t now = mKernel.sys.getTimeMs();
        uint32_t sleepMs = mEngine.execute(now);

        if (!mEngine.active()) {
            if (!mGuiStarted && static_cast<int32_t>(now - startTime) > static_cast<int32_t>(kStartupGraceMs)) {
                LOG_INFO("No GUI and no workout: exiting\n");
                break;
            }
            if (sleepMs > kIdlePollMs) {
                sleepMs = kIdlePollMs;
            }
        }

        SDK::MessageBase* msg = nullptr;
        if (mKernel.comm.getMessage(msg, sleepMs)) {
            switch (msg->getType()) {
                case SDK::MessageType::COMMAND_APP_STOP:
                    LOG_INFO("Force exit\n");
                    shutdown();
                    mKernel.comm.releaseMessage(msg);
                    return;

                case SDK::MessageType::COMMAND_APP_NOTIF_GUI_RUN:
                    onStartGUI();
                    break;

                case SDK::MessageType::COMMAND_APP_NOTIF_GUI_STOP:
                    onStopGUI();
                    break;

                case SDK::MessageType::EVENT_SENSOR_LAYER_DATA: {
                    auto* ev = static_cast<SDK::Message::Sensor::EventData*>(msg);
                    SDK::Sensor::DataBatch data(ev->data, ev->count, ev->stride);
                    onSensorData(ev->handle, data);
                } break;

                case CustomMessage::WO_START:
                    handleStart(*static_cast<CustomMessage::WorkoutStart*>(msg));
                    break;

                case CustomMessage::WO_CONTROL:
                    handleControl(*static_cast<CustomMessage::WorkoutControl*>(msg));
                    break;

                case CustomMessage::WO_SETTINGS_SAVE:
                    handleSettingsSave(*static_cast<CustomMessage::SettingsSave*>(msg));
                    break;

                default:
                    break;
            }
            mKernel.comm.releaseMessage(msg);
        }
    }

    shutdown();
}

// -- Lifecycle ----------------------------------------------------------------

void Service::onStartGUI()
{
    LOG_INFO("GUI running\n");
    mGuiStarted = true;
    sendSettings();
    if (mEngine.phase() == Phase::DONE) {
        SDK::send_msg<CustomMessage::Summary>(mKernel, mSummary);
    }
    sendState();
}

void Service::onStopGUI()
{
    LOG_INFO("GUI stopped%s\n", mEngine.active() ? " (workout keeps running)" : "");
    mGuiStarted = false;
}

void Service::shutdown()
{
    stopOutputs();
    hrStop();
}

// -- GUI commands ---------------------------------------------------------------

void Service::handleStart(const CustomMessage::WorkoutStart& msg)
{
    mCircuits  = msg.circuits;
    mAlertMode = static_cast<Workout::AlertMode>(msg.alertMode);
    if (mCircuits < Workout::kMinCircuits || mCircuits > Workout::kMaxCircuits) {
        mCircuits = 1;
    }
    if (msg.alertMode >= static_cast<uint8_t>(Workout::AlertMode::COUNT)) {
        mAlertMode = Workout::AlertMode::BEEP_AND_VIBRO;
    }
    saveSettings();

    LOG_INFO("Workout start: %u circuit(s), alerts %s\n", mCircuits, Workout::alertModeName(mAlertMode));
    mHrSum   = 0;
    mHrCount = 0;
    mHrMax   = 0;
    hrStart();
    mEngine.start(mCircuits, mKernel.sys.getTimeMs());
}

void Service::handleControl(const CustomMessage::WorkoutControl& msg)
{
    const uint32_t now = mKernel.sys.getTimeMs();
    using CustomMessage::Control;
    switch (msg.cmd) {
        case Control::TOGGLE_PAUSE: mEngine.togglePause(now); break;
        case Control::PAUSE:        mEngine.pause(now);       break;
        case Control::RESUME:       mEngine.resume(now);      break;
        case Control::NEXT:         mEngine.next(now);        break;
        case Control::PREV:         mEngine.prev(now);        break;
        case Control::FINISH:       mEngine.finish(now);      break;
        case Control::RESET:
            stopOutputs();
            hrStop();
            mEngine.reset();
            break;
    }
}

void Service::handleSettingsSave(const CustomMessage::SettingsSave& msg)
{
    if (msg.circuits >= Workout::kMinCircuits && msg.circuits <= Workout::kMaxCircuits) {
        mCircuits = msg.circuits;
    }
    if (msg.alertMode < static_cast<uint8_t>(Workout::AlertMode::COUNT)) {
        mAlertMode = static_cast<Workout::AlertMode>(msg.alertMode);
    }
    saveSettings();
}

// -- Engine::Listener -------------------------------------------------------------

void Service::onCue(Cue cue, uint8_t arg)
{
    const CuePattern* p = patternFor(cue);
    if (!p) {
        return;
    }
    LOG_INFO("CUE %s %u (t=%lu)\n", cueName(cue), arg, static_cast<unsigned long>(mKernel.sys.getTimeMs()));

    const bool beep  = mAlertMode != Workout::AlertMode::VIBRO_ONLY;
    const bool vibro = mAlertMode != Workout::AlertMode::BEEP_ONLY;

    if (p->backlightMs) {
        backlight(p->backlightMs);
    }

    if (vibro && p->buzzCount) {
        auto* m = mKernel.comm.allocateMessage<SDK::Message::RequestVibroPlay>();
        if (m) {
            for (uint8_t i = 0; i < p->buzzCount; ++i) {
                m->notes[i].effect = p->buzz[i].effect;
                m->notes[i].pause  = p->buzz[i].pauseMs;
            }
            m->notesCount = p->buzzCount;
            mKernel.comm.sendMessage(m);
            mKernel.comm.releaseMessage(m);
        }
    }

    if (beep && p->toneCount) {
        auto* m = mKernel.comm.allocateMessage<SDK::Message::RequestBuzzerPlay>();
        if (m) {
            for (uint8_t i = 0; i < p->toneCount; ++i) {
                m->notes[i].time      = p->tones[i].ms;
                m->notes[i].volume    = p->tones[i].volume;
                m->notes[i].frequency = p->tones[i].hz;
            }
            m->notesCount = p->toneCount;
            mKernel.comm.sendMessage(m);
            mKernel.comm.releaseMessage(m);
        }
    }
}

void Service::onStateChanged()
{
    const Phase phase = mEngine.phase();
    if (phase == Phase::DONE && mLastPhase != Phase::DONE) {
        hrStop();
        buildSummary();
        if (mGuiStarted) {
            SDK::send_msg<CustomMessage::Summary>(mKernel, mSummary);
        }
    }
    mLastPhase = phase;
    sendState();
}

// -- Helpers ------------------------------------------------------------------------

void Service::sendState()
{
    if (!mGuiStarted) {
        return;
    }
    const uint32_t now = mKernel.sys.getTimeMs();
    auto guard = SDK::make_msg<CustomMessage::WorkoutState>(mKernel);
    if (!guard) {
        return;
    }
    auto* s            = guard.get();
    s->phase           = static_cast<uint8_t>(mEngine.phase());
    s->paused          = mEngine.paused() ? 1 : 0;
    s->exercise        = mEngine.exercise();
    s->circuit         = mEngine.circuit();
    s->circuits        = mEngine.circuits();
    s->exercisesDone   = static_cast<uint8_t>(mEngine.exercisesDone());
    s->phaseDurationMs = mEngine.phaseDurationMs();
    s->endTick         = mEngine.endTick();
    s->remainingMs     = mEngine.remainingMs(now);
    guard.send();
}

void Service::sendSettings()
{
    if (!mGuiStarted) {
        return;
    }
    SDK::send_msg<CustomMessage::Settings>(mKernel, mCircuits, static_cast<uint8_t>(mAlertMode), mWorkoutsDone);
}

void Service::buildSummary()
{
    const uint32_t now = mKernel.sys.getTimeMs();
    if (mEngine.completed()) {
        ++mWorkoutsDone;
        saveSettings();
    }
    mSummary                  = CustomMessage::SummaryData();
    mSummary.activeMs         = mEngine.activeMs(now);
    mSummary.workMs           = mEngine.workMs(now);
    mSummary.workoutsDone     = mWorkoutsDone;
    mSummary.exercisesDone    = static_cast<uint8_t>(mEngine.exercisesDone());
    mSummary.exercisesPlanned = static_cast<uint8_t>(mEngine.circuits() * Workout::kExerciseCount);
    mSummary.circuits         = mEngine.circuits();
    mSummary.completed        = mEngine.completed() ? 1 : 0;
    mSummary.avgHr            = mHrCount ? static_cast<uint8_t>(mHrSum / mHrCount) : 0;
    mSummary.maxHr            = mHrMax;
    LOG_INFO("Summary: %lu ms active, %u/%u exercises, completed=%u, HR avg %u max %u\n",
             static_cast<unsigned long>(mSummary.activeMs), mSummary.exercisesDone,
             mSummary.exercisesPlanned, mSummary.completed, mSummary.avgHr, mSummary.maxHr);
}

void Service::hrStart()
{
    if (!mHrConnected) {
        mHrConnected = mSensorHR.connect();
        if (!mHrConnected) {
            LOG_WARNING("Heart-rate sensor unavailable\n");
        }
    }
    mHr = 0;
}

void Service::hrStop()
{
    if (mHrConnected) {
        mSensorHR.disconnect();
        mHrConnected = false;
    }
    mHr = 0;
}

void Service::onSensorData(uint16_t handle, SDK::Sensor::DataBatch& data)
{
    if (!mHrConnected || !mSensorHR.matchesDriver(handle) || data.size() == 0) {
        return;
    }
    SDK::SensorDataParser::HeartRate parser(data[data.size() - 1]);
    if (!parser.isDataValid()) {
        return;
    }
    const float bpmF = parser.getBpm();
    if (bpmF < 30.0f || bpmF > 240.0f) {
        return;
    }
    const uint8_t bpm = static_cast<uint8_t>(bpmF + 0.5f);

    if (mEngine.active() && !mEngine.paused()) {
        mHrSum += bpm;
        ++mHrCount;
        if (bpm > mHrMax) {
            mHrMax = bpm;
        }
    }
    if (bpm != mHr) {
        mHr = bpm;
        if (mGuiStarted) {
            SDK::send_msg<CustomMessage::HeartRate>(mKernel, bpm);
        }
    }
}

void Service::backlight(uint32_t onMs)
{
    auto* m = mKernel.comm.allocateMessage<SDK::Message::RequestBacklightSet>();
    if (m) {
        m->brightness       = 100;
        m->autoOffTimeoutMs = onMs;
        mKernel.comm.sendMessage(m);
        mKernel.comm.releaseMessage(m);
    }
}

void Service::stopOutputs()
{
    // Empty play requests silence the buzzer and stop the vibration motor.
    auto* b = mKernel.comm.allocateMessage<SDK::Message::RequestBuzzerPlay>();
    if (b) {
        mKernel.comm.sendMessage(b);
        mKernel.comm.releaseMessage(b);
    }
    auto* v = mKernel.comm.allocateMessage<SDK::Message::RequestVibroPlay>();
    if (v) {
        mKernel.comm.sendMessage(v);
        mKernel.comm.releaseMessage(v);
    }
}

// -- Settings file: "circuits=2\nalert=0\ndone=14\n" ------------------------------

void Service::loadSettings()
{
    auto file = mKernel.fs.file(kSettingsPath);
    if (!file || !file->exist()) {
        LOG_INFO("No settings yet, using defaults\n");
        return;
    }
    char   buf[96] = {};
    size_t br      = 0;
    if (file->open()) {
        file->read(buf, sizeof(buf) - 1, br);
        file->close();
    }
    buf[br < sizeof(buf) ? br : sizeof(buf) - 1] = '\0';

    // Tiny "key=value" parser (no sscanf: keeps newlib's stdio out of the image).
    auto valueOf = [&buf](const char* key, unsigned& out) -> bool {
        const char* p = std::strstr(buf, key);
        if (!p) {
            return false;
        }
        p += std::strlen(key);
        if (*p < '0' || *p > '9') {
            return false;
        }
        unsigned v = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10u + static_cast<unsigned>(*p++ - '0');
        }
        out = v;
        return true;
    };

    unsigned c = 0, a = 0, d = 0;
    if (valueOf("circuits=", c) && c >= Workout::kMinCircuits && c <= Workout::kMaxCircuits) {
        mCircuits = static_cast<uint8_t>(c);
    }
    if (valueOf("alert=", a) && a < static_cast<unsigned>(Workout::AlertMode::COUNT)) {
        mAlertMode = static_cast<Workout::AlertMode>(a);
    }
    if (valueOf("done=", d) && d <= 0xFFFFu) {
        mWorkoutsDone = static_cast<uint16_t>(d);
    }
    LOG_INFO("Settings: circuits=%u alert=%u done=%u\n", mCircuits,
             static_cast<unsigned>(mAlertMode), mWorkoutsDone);
}

void Service::saveSettings()
{
    char buf[96];
    const int len = std::snprintf(buf, sizeof(buf), "circuits=%u\nalert=%u\ndone=%u\n",
                                  mCircuits, static_cast<unsigned>(mAlertMode), mWorkoutsDone);
    if (len <= 0) {
        return;
    }
    auto file = mKernel.fs.file(kSettingsPath);
    if (!file) {
        return;
    }
    size_t bw = 0;
    if (file->open(true, true)) {
        file->write(buf, static_cast<size_t>(len), bw);
        file->close();
    }
    if (bw != static_cast<size_t>(len)) {
        LOG_WARNING("Settings not saved\n");
    }
}
