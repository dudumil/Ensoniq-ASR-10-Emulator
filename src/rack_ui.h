// Front panel of the rack unit for the SDL front end: faceplate, buttons, LEDs, sliders,
// display window, floppy drive and the printed legends.
//
// Layout is kept in "panel units" (a 1870 x 515 panel with the rack ears, measured on a
// photo of the rack front) and scaled to logical window pixels.
#pragma once
#include <SDL.h>
#include <cstdint>
#include <vector>

class Panel;
class VfdRenderer;

struct RackState {
    const Panel *panel = nullptr;
    uint32_t ticks = 0;
    const bool *held = nullptr;        // held[panel button index], 64 entries
    float volume = 1, data = 0.5f;     // slider positions 0..1 (1 = top)
    bool disk = false, drive_busy = false;
    uint32_t scsi_activity = 0;        // SCSI bus activity counter: the LED over the floppy drive blinks when it changes
    float in_left = 0, in_right = 0;   // input peak since the last frame (0..1)
    bool power_held = false;
    bool audio_led[4] = {};            // Audio Track A/B: Selected (yellow), Source Monitor (red)
};

class RackUI {
public:
    static constexpr float PW = 1870, PH = 515;          // panel units
    static constexpr float OX = 0, OY = 4;               // where the panel goes in the window
    explicit RackUI(float width) : K(width / PW) {}
    float height() const { return PH * K; }
    float X(float u) const { return OX + (u - 85) * K; }
    float Y(float v) const { return OY + (v - 155) * K; }

    bool init(SDL_Renderer *r);
    ~RackUI();
    void draw(const RackState &s, VfdRenderer &vfd);

    // hit testing (logical window coordinates)
    int button_at(int x, int y) const;                   // panel button index, AUDIO_A/AUDIO_B or -1
    static constexpr int AUDIO_A = 40, AUDIO_B = 41;     // the Audio Track buttons (not on the panel micro)
    enum Area { NONE, POWER, EJECT, DRIVE };
    Area area_at(int x, int y) const;
    // slider geometry for the mouse: top of the knob travel and its length
    struct Slider { float x, y, w, h; };
    Slider volume_slider() const { return slider(VOL_X, VOL_TOP, VOL_BOT); }
    Slider data_slider() const { return slider(DATA_X, DATA_TOP, DATA_BOT); }

private:
    float K;
    uint32_t scsi_seen = 0, scsi_seen_t = 0;   // last activity count and when it changed (LED hold)
    bool scsi_seen_init = false, scsi_hit = false;
    SDL_Renderer *R = nullptr;
    SDL_Texture *plate = nullptr, *glow = nullptr;
    std::vector<SDL_Texture *> legend;

    struct Btn { int idx; float x, y, w, h; };
    std::vector<Btn> buttons;
    static constexpr float VOL_X = 346, VOL_TOP = 368, VOL_BOT = 556;   // same travel as Data Entry
    static constexpr float DATA_X = 1609, DATA_TOP = 368, DATA_BOT = 556;

    SDL_FRect rect(float x, float y, float w, float h) const { return { X(x), Y(y), w * K, h * K }; }
    Slider slider(float cx, float top, float bot) const;
    void fill(const SDL_FRect &r, uint32_t c, uint8_t a = 255);
    void vgrad(const SDL_FRect &r, uint32_t top, uint32_t bot);
    void circle(float cx, float cy, float rad, uint32_t c);
    void line(float x1, float y1, float x2, float y2, uint32_t c);
    void led(float cx, float cy, float w, float h, uint32_t on_colour, bool on);
    void button(const Btn &b, bool pressed);
    void slider_knob(float cx, float top, float bot, float val);
};
