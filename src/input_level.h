#pragma once
// Input Level pot (on the real ASR-10 rack it is on the back panel; here it is on the front, left of
// the input LEDs). Position 0..1: 0 = off, 2/3 = 0 dB (unity), 1 = +18 dB. The gain is applied
// before the A/D, so the input LEDs and the OS level meter follow it and loud input clips.
#include <algorithm>
#include <cmath>

constexpr float INPUT_LEVEL_0DB = 2.0f / 3;
inline float input_level_db(float pos) {
    pos = std::clamp(pos, 0.0f, 1.0f);
    return pos <= 0.005f ? -100.0f : -36.0f + pos * 54.0f;
}
inline float input_level_gain(float pos) {
    float db = input_level_db(pos);
    return db <= -99 ? 0.0f : std::pow(10.0f, db / 20.0f);
}
