#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fb {

// One step of an iteration count: about 25% per step so a few presses cross the range, but
// at least 1 so small counts still move. The clamp is widened to include `v`, so a value set
// out of range (e.g. from the command line) never jumps the wrong way.
inline uint32_t stepIterations(uint32_t v, int dir, uint32_t lo, uint32_t hi) {
    if (dir == 0) return v;
    const double x = double(v);
    const double next = dir > 0 ? std::max(x + 1.0, double(std::lround(x * 1.25)))
                                : std::min(x - 1.0, double(std::lround(x / 1.25)));
    return uint32_t(std::clamp(next, double(std::min(lo, v)), double(std::max(hi, v))));
}

// Key repeat for held keys, timed by the app instead of the OS (whose repeat rate can be far
// too fast for stepping): one step on the press, then after kDelay one every kInterval.
struct KeyRepeat {
    static constexpr double kDelay = 0.35;
    static constexpr double kInterval = 0.10;

    // Feeds this frame's key state; returns how many steps fire this frame.
    int update(bool down, float dt) {
        if (!down) {
            m_held = -1.0;
            return 0;
        }
        if (m_held < 0.0) {
            m_held = 0.0;
            m_next = kDelay;
            return 1;
        }
        m_held += double(dt);
        int steps = 0;
        // Small slack so accumulated frame times landing a hair short still fire on time.
        while (m_held >= m_next - 1e-9) {
            ++steps;
            m_next += kInterval;
        }
        return steps;
    }

private:
    double m_held = -1.0; // seconds the key has been down, < 0 while up
    double m_next = 0.0;  // held time of the next repeat
};

} // namespace fb
