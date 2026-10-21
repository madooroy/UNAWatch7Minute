/**
 * Host unit tests for Workout::Engine. No SDK needed:
 *   cmake -S Tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
 */

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "WorkoutEngine.hpp"

using namespace Workout;

static int gFailures = 0;
#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
            ++gFailures;                                                            \
        }                                                                           \
    } while (0)
#define CHECK_EQ(a, b)                                                              \
    do {                                                                            \
        long long _a = (long long)(a), _b = (long long)(b);                         \
        if (_a != _b) {                                                             \
            std::printf("  FAIL %s:%d: %s == %s (%lld vs %lld)\n", __FILE__,        \
                        __LINE__, #a, #b, _a, _b);                                  \
            ++gFailures;                                                            \
        }                                                                           \
    } while (0)

struct Event {
    uint32_t t;   // ms since start
    Cue      cue;
    uint8_t  arg;
};

struct Recorder : Engine::Listener {
    uint32_t           base = 0;
    uint32_t*          clock = nullptr;
    std::vector<Event> events;
    int                stateChanges = 0;
    void onCue(Cue c, uint8_t a) override { events.push_back({*clock - base, c, a}); }
    void onStateChanged() override { ++stateChanges; }

    int count(Cue c) const
    {
        int n = 0;
        for (auto& e : events) n += (e.cue == c);
        return n;
    }
    const Event* nth(Cue c, int n) const
    {
        for (auto& e : events) {
            if (e.cue == c && n-- == 0) return &e;
        }
        return nullptr;
    }
};

/// Run the engine the way the service does: sleep exactly what execute() asks
/// (plus optional jitter) until the workout is over.
static void runToEnd(Engine& e, uint32_t& now, uint32_t jitter = 0)
{
    for (int i = 0; i < 100000 && e.active(); ++i) {
        uint32_t w = e.execute(now);
        if (w == Engine::kNoWake) break;
        now += w + jitter;
    }
    e.execute(now);
}

static void testSingleCircuitTimeline(uint32_t startTick)
{
    std::printf("single circuit, start tick 0x%08X\n", startTick);
    Engine   e;
    Recorder r;
    uint32_t now = startTick;
    r.base = startTick;
    r.clock = &now;
    e.setListener(&r);

    e.start(1, now);
    CHECK(e.phase() == Phase::GET_READY);
    CHECK_EQ(e.remainingMs(now), kGetReadyMs);

    runToEnd(e, now);

    CHECK(e.phase() == Phase::DONE);
    CHECK(e.completed());
    CHECK_EQ(e.exercisesDone(), 12);
    CHECK_EQ(now - startTick, plannedDurationMs(1));        // 8:00 exactly
    CHECK_EQ(e.activeMs(now), plannedDurationMs(1));
    CHECK_EQ(e.workMs(now), 12u * kWorkMs);

    // 12 GOs: first at the end of the lead-in, then every 40 s.
    CHECK_EQ(r.count(Cue::GO), 12);
    for (int i = 0; i < 12; ++i) {
        const Event* g = r.nth(Cue::GO, i);
        CHECK(g && g->t == kGetReadyMs + i * (kWorkMs + kRestMs));
    }
    // 11 rests, 1 finish, no circuit-done for a single circuit.
    CHECK_EQ(r.count(Cue::REST), 11);
    CHECK_EQ(r.count(Cue::FINISHED), 1);
    CHECK_EQ(r.count(Cue::CIRCUIT_DONE), 0);
    const Event* rest0 = r.nth(Cue::REST, 0);
    CHECK(rest0 && rest0->t == kGetReadyMs + kWorkMs);

    // Countdown 3-2-1 before every phase end: lead-in + 12 work + 11 rest = 24 phases.
    CHECK_EQ(r.count(Cue::COUNTDOWN), 24 * 3);
    const Event* c0 = r.nth(Cue::COUNTDOWN, 0);
    const Event* c1 = r.nth(Cue::COUNTDOWN, 1);
    const Event* c2 = r.nth(Cue::COUNTDOWN, 2);
    CHECK(c0 && c0->t == 7000 && c0->arg == 3);
    CHECK(c1 && c1->t == 8000 && c1->arg == 2);
    CHECK(c2 && c2->t == 9000 && c2->arg == 1);

    // Side plank (exercise 12) switches sides half-way.
    CHECK_EQ(r.count(Cue::SWITCH_SIDES), 1);
    const Event* sw = r.nth(Cue::SWITCH_SIDES, 0);
    CHECK(sw && sw->t == kGetReadyMs + 11 * (kWorkMs + kRestMs) + kWorkMs / 2);
}

static void testThreeCircuits()
{
    std::printf("three circuits\n");
    Engine   e;
    Recorder r;
    uint32_t now = 1000;
    r.base = now;
    r.clock = &now;
    e.setListener(&r);

    e.start(3, now);
    runToEnd(e, now);
    CHECK(e.completed());
    CHECK_EQ(e.exercisesDone(), 36);
    CHECK_EQ(now - 1000, plannedDurationMs(3));
    CHECK_EQ(r.count(Cue::GO), 36);
    CHECK_EQ(r.count(Cue::CIRCUIT_DONE), 2);
    CHECK_EQ(r.count(Cue::REST), 33);
    CHECK_EQ(r.count(Cue::SWITCH_SIDES), 3);
    const Event* cd = r.nth(Cue::CIRCUIT_DONE, 1);
    CHECK(cd && cd->arg == 2);
}

static void testCircuitClamp()
{
    std::printf("circuit clamp\n");
    Engine e;
    e.start(0, 0);
    CHECK_EQ(e.circuits(), 1);
    e.start(9, 0);
    CHECK_EQ(e.circuits(), 3);
}

static void testPauseResume()
{
    std::printf("pause / resume\n");
    Engine   e;
    Recorder r;
    uint32_t now = 0;
    r.clock = &now;
    e.setListener(&r);

    e.start(1, now);
    now = 15000;          // 5 s into exercise 1
    e.execute(now);
    CHECK(e.phase() == Phase::WORK);
    CHECK_EQ(e.remainingMs(now), 25000);

    e.pause(now);
    CHECK(e.paused());
    CHECK_EQ(e.execute(now), Engine::kNoWake);
    now += 60000;         // a minute's break
    CHECK_EQ(e.remainingMs(now), 25000);
    CHECK(e.phase() == Phase::WORK);

    e.resume(now);
    CHECK_EQ(e.remainingMs(now), 25000);
    CHECK_EQ(r.count(Cue::PAUSED), 1);
    CHECK_EQ(r.count(Cue::RESUMED), 1);

    runToEnd(e, now);
    CHECK(e.completed());
    CHECK_EQ(e.activeMs(now), plannedDurationMs(1));        // pause excluded
    CHECK_EQ(now, plannedDurationMs(1) + 60000);
    CHECK_EQ(e.workMs(now), 12u * kWorkMs);
}

static void testSkip()
{
    std::printf("skip next / prev\n");
    Engine   e;
    Recorder r;
    uint32_t now = 0;
    r.clock = &now;
    e.setListener(&r);

    e.start(2, now);
    e.next(now);                                 // skip lead-in
    CHECK(e.phase() == Phase::WORK);
    CHECK_EQ(e.exercise(), 0);
    CHECK_EQ(r.count(Cue::GO), 1);

    now += 10000;
    e.next(now);                                 // skip ex1 -> rest before ex2
    CHECK(e.phase() == Phase::REST);
    CHECK_EQ(e.exercise(), 1);
    CHECK_EQ(e.exercisesDone(), 0);              // skipped: not counted

    e.prev(now);                                 // from rest: redo ex1
    CHECK(e.phase() == Phase::WORK);
    CHECK_EQ(e.exercise(), 0);

    now += 10000;
    e.prev(now);                                 // 10 s in: restart ex1
    CHECK(e.phase() == Phase::WORK);
    CHECK_EQ(e.exercise(), 0);
    CHECK_EQ(e.remainingMs(now), kWorkMs);

    // Walk to the last exercise of circuit 1, then step back across the boundary.
    for (int i = 0; i < 12; ++i) {
        e.next(now);   // work -> rest
        if (e.phase() == Phase::REST && e.circuit() == 2) break;
        e.next(now);   // rest -> work
    }
    CHECK(e.phase() == Phase::REST);
    CHECK_EQ(e.circuit(), 2);
    CHECK_EQ(e.exercise(), 0);
    e.prev(now);
    CHECK(e.phase() == Phase::WORK);
    CHECK_EQ(e.circuit(), 1);
    CHECK_EQ(e.exercise(), 11);

    // Skip while paused stays paused with a full interval queued.
    e.pause(now);
    e.next(now);
    CHECK(e.paused());
    CHECK(e.phase() == Phase::REST);
    CHECK_EQ(e.remainingMs(now + 5000), kRestMs);
}

static void testFinishEarlyAndReset()
{
    std::printf("finish early / reset\n");
    Engine   e;
    Recorder r;
    uint32_t now = 0;
    r.clock = &now;
    e.setListener(&r);
    e.start(1, now);
    now = 10000 + 30000 + 10000 + 12000;         // 12 s into exercise 2
    e.execute(now);
    CHECK_EQ(e.exercise(), 1);
    e.finish(now);
    CHECK(e.phase() == Phase::DONE);
    CHECK(!e.completed());
    CHECK_EQ(e.exercisesDone(), 1);
    CHECK_EQ(e.activeMs(now), now);
    CHECK_EQ(e.workMs(now), 42000);
    CHECK_EQ(e.execute(now + 100000), Engine::kNoWake);
    e.reset();
    CHECK(e.phase() == Phase::IDLE);
}

static void testLateWakeDoesNotDrift()
{
    std::printf("late wake-ups do not drift\n");
    Engine   e;
    Recorder r;
    uint32_t now = 500;
    r.base = now;
    r.clock = &now;
    e.setListener(&r);
    e.start(1, now);
    runToEnd(e, now, 37);                        // every wake 37 ms late
    CHECK(e.completed());
    // Phase boundaries still land on schedule: the end is late by at most one jitter.
    CHECK((now - 500) >= plannedDurationMs(1));
    CHECK((now - 500) <= plannedDurationMs(1) + 37 + 1000);
    CHECK_EQ(r.count(Cue::GO), 12);
    CHECK_EQ(r.count(Cue::COUNTDOWN), 72);
}

static void testVeryLateWake()
{
    std::printf("a wake late by several phases catches up\n");
    Engine   e;
    Recorder r;
    uint32_t now = 0;
    r.clock = &now;
    e.setListener(&r);
    e.start(1, now);
    now = 10000 + 30000 + 10000 + 30000 + 5000;  // into rest before ex3
    e.execute(now);
    CHECK(e.phase() == Phase::REST);
    CHECK_EQ(e.exercise(), 2);
    CHECK_EQ(e.remainingMs(now), 5000);
}

static void testSkippingIsNotCompleting()
{
    std::printf("skipping to the end does not count as completed\n");
    Engine   e;
    Recorder r;
    uint32_t now = 0;
    r.clock = &now;
    e.setListener(&r);
    e.start(1, now);
    e.next(now);                                  // into exercise 1
    for (int i = 0; i < 22; ++i) e.next(now);     // skip to the side plank
    CHECK(e.phase() == Phase::WORK);
    CHECK_EQ(e.exercise(), 11);
    runToEnd(e, now);
    CHECK(e.phase() == Phase::DONE);
    CHECK_EQ(e.exercisesDone(), 1);
    CHECK(!e.completed());

    // Skipping just two of twelve still counts.
    e.start(1, now);
    e.next(now);
    runToEnd(e, now);                             // lead-in skipped only
    CHECK(e.completed());
    e.start(1, now);
    now += 15000; e.execute(now);                 // into exercise 1
    e.next(now); e.next(now);                     // skip ex1
    e.next(now); e.next(now);                     // skip ex2
    runToEnd(e, now);
    CHECK_EQ(e.exercisesDone(), 10);
    CHECK(e.completed());
}

int main()
{
    testSingleCircuitTimeline(0);
    testSingleCircuitTimeline(0xFFFFFFFFu - 100000u);   // tick wraps mid-workout
    testThreeCircuits();
    testCircuitClamp();
    testPauseResume();
    testSkip();
    testFinishEarlyAndReset();
    testLateWakeDoesNotDrift();
    testVeryLateWake();
    testSkippingIsNotCompleting();

    if (gFailures) {
        std::printf("%d check(s) FAILED\n", gFailures);
        return EXIT_FAILURE;
    }
    std::printf("all engine tests passed\n");
    return EXIT_SUCCESS;
}
