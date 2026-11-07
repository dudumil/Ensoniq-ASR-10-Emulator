// Rack front panel drawing - see rack_ui.h.
#include "rack_ui.h"
#include "panel_art.h"
#include "panel.h"
#include "vfd.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace {
// colours of the printed legends (see tools/make_panel_art.py)
const uint32_t LEGEND_COLOUR[4] = { 0xd2d3d5, 0xd8b25a, 0x3f9bd0, 0xa7a9ad };
const uint32_t BG = 0x101112;

uint32_t mix(uint32_t a, uint32_t b, float t) {
    auto ch = [&](int s) { float x = ((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t; return (uint32_t)(x + 0.5f) << s; };
    return ch(16) | ch(8) | ch(0);
}
// small deterministic noise
uint32_t hash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16; return x; }
float rnd(uint32_t x) { return (hash(x) & 0xffff) / 65535.0f; }
}

RackUI::~RackUI() {
    if (plate) SDL_DestroyTexture(plate);
    if (glow) SDL_DestroyTexture(glow);
    for (auto *t : legend) if (t) SDL_DestroyTexture(t);
}

bool RackUI::init(SDL_Renderer *r) {
    R = r;
    // ---- buttons (panel button index, rectangle in panel units)
    // buttons that sit side by side touch, with just the 1-unit seam between them (as on the EPS panel)
    const float cx[3] = { 423.75f, 486.25f, 548.75f }, ry[4] = { 220, 291, 362, 433 };   // pitch 62.5 x 71
    const int par[12] = { 13, 18, 19, 24, 25, 30, 31, 36, 37, 32, 12, 7 };   // Env1..Layer, Sample, Track(0), FX Select
    for (int i = 0; i < 12; i++) buttons.push_back({ par[i], cx[i % 3], ry[i / 3], 61.5f, 25 });
    // Audio Track A/B: not panel buttons, they are wired to 68302 PB5/PB6 (see RackUI::AUDIO_A)
    buttons.push_back({ AUDIO_A, 423.75f, 535, 61.5f, 42 });   // under Sample, level with the instrument buttons
    buttons.push_back({ AUDIO_B, 548.75f, 535, 61.5f, 42 });   // under FX Select
    const int mode[3] = { 26, 6, 5 };                       // Load, Command, Edit
    for (int i = 0; i < 3; i++) buttons.push_back({ mode[i], 735.2f + i * 61.6f, 433, 60.6f, 25 });   // in line with Instruments 1-3
    const int mode2[4] = { 15, 21, 27, 9 };                 // Instrument, Seq/Song, System/MIDI, Effects
    for (int i = 0; i < 4; i++) buttons.push_back({ mode2[i], 735.2f + (4 + i) * 61.6f, 433, 60.6f, 25 });   // with 5-8
    const int trk[8] = { 2, 8, 14, 20, 4, 34, 28, 22 };
    for (int i = 0; i < 8; i++) buttons.push_back({ trk[i], 735.2f + i * 61.6f, 535, 60.6f, 42 });
    buttons.push_back({ 3, 1347, 220, 61, 25 });            // Record
    buttons.push_back({ 23, 1409, 220, 61, 25 });           // Stop/Continue
    buttons.push_back({ 29, 1471, 220, 61, 25 });           // Play
    buttons.push_back({ 10, 1409, 362, 61, 25 });           // up
    buttons.push_back({ 16, 1347, 433, 61, 25 });           // left
    buttons.push_back({ 11, 1409, 433, 61, 25 });           // down
    buttons.push_back({ 17, 1471, 433, 61, 25 });           // right
    buttons.push_back({ 33, 1347, 535, 61, 42 });           // Cancel/No
    buttons.push_back({ 35, 1471, 535, 61, 42 });           // Enter/Yes

    // ---- brushed anodised faceplate (with the rack ears and their slots cut out)
    const int tw = (int)std::ceil(PW * K * 2), th = (int)std::ceil(PH * K * 2);
    const float ts = tw / PW;                                // texels per panel unit
    std::vector<uint32_t> px((size_t)tw * th);
    std::vector<float> rowv(th);
    for (int y = 0; y < th; y++) rowv[y] = (rnd(y * 7919u) - 0.5f) * 0.9f + (rnd(y / 3 * 104729u) - 0.5f) * 0.6f;
    for (int y = 0; y < th; y++) {
        float v = y / ts;                                    // panel units from the top
        for (int x = 0; x < tw; x++) {
            float u = x / ts + 85;
            bool ear = u < 178 || u > 1860;
            float base = ear ? 40.0f : 36.0f;
            // fine horizontal brushing: a slowly varying streak per row plus a little grain
            float streak = rowv[y] * 3.0f + (rnd(y * 131u + (x >> 5) * 977u) - 0.5f) * 1.2f;
            float grain = (rnd(x * 2654435761u ^ (y * 40503u)) - 0.5f) * 2.0f;
            float l = base + streak + grain;
            // bevels: lit top edge, dark bottom edge, groove between ears and panel
            if (v < 2.5f) l += 26 * (1 - v / 2.5f);
            if (v > PH - 3) l -= 14;
            if (std::fabs(u - 178) < 2.2f || std::fabs(u - 1860) < 2.2f) l -= 12;
            if (std::fabs(u - 181) < 1.2f || std::fabs(u - 1863) < 1.2f) l += 8;
            int c = std::clamp((int)l, 0, 255);
            uint32_t a = 255;
            // rack mounting slots (two per ear)
            const float hx[2] = { 120, 1920 }, hy[2] = { 305, 523 };
            for (float ex : hx) for (float ey : hy) {
                float dx = (u - ex) / 21.0f, dy = (v + 155 - ey) / 12.5f;
                float d = std::fabs(dx) > 0.45f ? std::sqrt((std::fabs(dx) - 0.45f) * (std::fabs(dx) - 0.45f) / (0.55f * 0.55f) + dy * dy) : std::fabs(dy);
                if (d < 1.0f) a = 0;
                else if (d < 1.25f) c = std::max(0, c - 10);
            }
            px[(size_t)y * tw + x] = (a << 24) | (uint32_t)(c << 16) | (uint32_t)(c << 8) | (uint32_t)(c + 2 > 255 ? 255 : c + 2);
        }
    }
    plate = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, tw, th);
    if (!plate) return false;
    SDL_UpdateTexture(plate, nullptr, px.data(), tw * 4);
    SDL_SetTextureBlendMode(plate, SDL_BLENDMODE_BLEND);

    // ---- soft round glow for the LEDs
    const int gs = 32;
    std::vector<uint32_t> g(gs * gs);
    for (int y = 0; y < gs; y++) for (int x = 0; x < gs; x++) {
        float dx = (x + 0.5f) / gs * 2 - 1, dy = (y + 0.5f) / gs * 2 - 1, d = std::sqrt(dx * dx + dy * dy);
        float a = d >= 1 ? 0 : (1 - d) * (1 - d);
        g[y * gs + x] = ((uint32_t)(a * 255) << 24) | 0xffffff;
    }
    glow = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, gs, gs);
    if (!glow) return false;
    SDL_UpdateTexture(glow, nullptr, g.data(), gs * 4);
    SDL_SetTextureBlendMode(glow, SDL_BLENDMODE_ADD);

    // ---- legends
    for (const PanelLegend &l : PANEL_LEGENDS) {
        std::vector<uint32_t> a((size_t)l.tw * l.th);
        for (size_t k = 0; k < a.size(); k++) {
            uint8_t b = PANEL_LEGEND_ALPHA[l.offset + k / 2];
            uint32_t v = ((k & 1) ? b >> 4 : b & 15) * 17;
            a[k] = (v << 24) | 0xffffff;
        }
        SDL_Texture *t = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, l.tw, l.th);
        if (!t) return false;
        SDL_UpdateTexture(t, nullptr, a.data(), l.tw * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        uint32_t c = LEGEND_COLOUR[l.colour & 3];
        SDL_SetTextureColorMod(t, c >> 16, (c >> 8) & 255, c & 255);
        legend.push_back(t);
    }
    return true;
}

// ------------------------------------------------------------------ primitives
void RackUI::fill(const SDL_FRect &r, uint32_t c, uint8_t a) {
    SDL_SetRenderDrawBlendMode(R, a == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(R, c >> 16, (c >> 8) & 255, c & 255, a);
    SDL_RenderFillRectF(R, &r);
}
void RackUI::vgrad(const SDL_FRect &r, uint32_t top, uint32_t bot) {
    int n = std::max(1, (int)std::ceil(r.h));
    for (int i = 0; i < n; i++) {
        float y0 = r.y + r.h * i / n, y1 = r.y + r.h * (i + 1) / n;
        fill({ r.x, y0, r.w, y1 - y0 }, mix(top, bot, (i + 0.5f) / n));
    }
}
void RackUI::circle(float cx, float cy, float rad, uint32_t c) {
    int n = std::max(2, (int)std::ceil(rad * 2));
    for (int i = 0; i < n; i++) {
        float y = -rad + (i + 0.5f) * 2 * rad / n, hw = std::sqrt(std::max(0.0f, rad * rad - y * y));
        fill({ cx - hw, cy + y - rad / n, 2 * hw, 2 * rad / n }, c);
    }
}
void RackUI::line(float x1, float y1, float x2, float y2, uint32_t c) {
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(R, c >> 16, (c >> 8) & 255, c & 255, 255);
    SDL_RenderDrawLineF(R, X(x1), Y(y1), X(x2), Y(y2));
}
void RackUI::led(float cx, float cy, float w, float h, uint32_t on_colour, bool on) {
    SDL_FRect r = rect(cx - w / 2, cy - h / 2, w, h);
    fill({ r.x - K * 1.5f, r.y - K * 1.5f, r.w + K * 3, r.h + K * 3 }, 0x0a0a0b);   // bezel
    if (!on) { vgrad(r, mix(on_colour, 0x000000, 0.80f), mix(on_colour, 0x000000, 0.88f)); return; }
    vgrad(r, mix(on_colour, 0xffffff, 0.35f), on_colour);
    float gw = w * 3.2f * K, gh = h * 4.5f * K;
    SDL_FRect g{ X(cx) - gw / 2, Y(cy) - gh / 2, gw, gh };
    SDL_SetTextureColorMod(glow, on_colour >> 16, (on_colour >> 8) & 255, on_colour & 255);
    SDL_SetTextureAlphaMod(glow, 150);
    SDL_RenderCopyF(R, glow, nullptr, &g);
}
void RackUI::button(const Btn &b, bool pressed) {
    SDL_FRect r = rect(b.x, b.y, b.w, b.h);
    float d = pressed ? K * 1.2f : 0;
    fill({ r.x - K, r.y - K, r.w + 2 * K, r.h + 3.5f * K }, 0x060607);            // recess and shadow
    SDL_FRect f{ r.x, r.y + d, r.w, r.h };
    vgrad(f, pressed ? 0x1f2022 : 0x313236, pressed ? 0x141516 : 0x1b1c1e);
    fill({ f.x, f.y, f.w, K * 1.3f }, pressed ? 0x2a2b2e : 0x4a4c51);             // lit top edge
    fill({ f.x, f.y + f.h - K * 1.6f, f.w, K * 1.6f }, 0x0e0f10);                 // lower edge
    fill({ f.x, f.y, K, f.h }, 0x26272a);
    fill({ f.x + f.w - K, f.y, K, f.h }, 0x111214);
}
RackUI::Slider RackUI::slider(float cx, float top, float bot) const {
    return { X(cx - 28), Y(top), 56 * K, (bot - top) * K };
}
void RackUI::slider_knob(float cx, float top, float bot, float val) {
    // slot
    SDL_FRect s = rect(cx - 6, top - 6, 12, bot - top + 12);
    fill({ s.x - K, s.y - K, s.w + 2 * K, s.h + 2 * K }, 0x2a2b2e);
    fill(s, 0x030303);
    // cap
    float cy = bot - (bot - top) * std::clamp(val, 0.0f, 1.0f);
    SDL_FRect c = rect(cx - 27, cy - 20, 54, 40);
    fill({ c.x, c.y + K * 2, c.w, c.h + K * 2 }, 0x050505);
    vgrad(c, 0x3a3b3f, 0x141517);
    fill({ c.x, c.y, c.w, K * 1.4f }, 0x5a5c61);
    SDL_FRect mid = rect(cx - 22, cy - 1.6f, 44, 3.2f);
    fill(mid, 0xe8e8e8);
}

// ------------------------------------------------------------------ hit testing
int RackUI::button_at(int x, int y) const {
    for (const Btn &b : buttons) {
        SDL_FRect r = rect(b.x - 2, b.y - 2, b.w + 4, b.h + 4);
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return b.idx;
    }
    return -1;
}
RackUI::Area RackUI::area_at(int x, int y) const {
    auto in = [&](SDL_FRect r) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; };
    if (in(rect(236, 405, 65, 90))) return POWER;
    if (in(rect(1758, 268, 31, 68))) return EJECT;
    if (in(rect(1695, 215, 103, 395))) return DRIVE;
    return NONE;
}

// ------------------------------------------------------------------ the panel
void RackUI::draw(const RackState &s, VfdRenderer &vfd) {
    const Panel &p = *s.panel;
    const bool blink = (s.ticks / 250) & 1;
    // faceplate
    SDL_FRect pl{ X(85), Y(155), PW * K, PH * K };
    SDL_RenderCopyF(R, plate, nullptr, &pl);
    // screw heads in the ear slots are not drawn: the slots show what is behind the rack

    // ---- input level LEDs (peak red, signal green), power switch, phones jack, volume
    const float sig = 0.063f, pk = 0.5f;           // manual: Signal at -24 dB, Peak 6 dB below clipping
    led(288, 230, 14, 8, 0xff2a1a, s.in_left >= pk);
    led(348, 230, 14, 8, 0xff2a1a, s.in_right >= pk);
    led(288, 258, 14, 8, 0x40ff40, s.in_left >= sig);
    led(348, 258, 14, 8, 0x40ff40, s.in_right >= sig);
    {   // rocker switch
        SDL_FRect o = rect(240, 410, 57, 80);
        fill({ o.x - K * 2, o.y - K * 2, o.w + K * 4, o.h + K * 4 }, 0x2c2d30);
        fill(o, 0x060606);
        SDL_FRect top = rect(246, 416, 45, 34), bot = rect(246, 450, 45, 34);
        if (s.power_held) { vgrad(top, 0x1a1b1d, 0x222326); vgrad(bot, 0x2a2b2e, 0x101113); }
        else { vgrad(top, 0x3a3b3f, 0x202124); vgrad(bot, 0x141516, 0x0c0c0d); }
        fill(rect(246, 449, 45, 1.5f), 0x050505);
    }
    circle(X(268), Y(560), 25 * K, 0x6f7175);         // phones jack: nut, ring, hole
    circle(X(268), Y(560), 21 * K, 0xa9abb0);
    circle(X(268), Y(560), 15 * K, 0x2a2b2d);
    circle(X(268), Y(560), 11 * K, 0x020202);
    slider_knob(VOL_X, VOL_TOP, VOL_BOT, s.volume);

    // ---- buttons
    for (const Btn &b : buttons) button(b, s.held && s.held[b.idx & 63]);

    // ---- LEDs: audio tracks (state not known yet), instruments/tracks (red = loaded, yellow = selected)
    led(454.5f, 512, 14, 8, 0xffc020, s.audio_led[0]); led(579.5f, 512, 14, 8, 0xffc020, s.audio_led[1]);   // Selected
    led(454.5f, 600, 14, 8, 0xff2a1a, s.audio_led[2]); led(579.5f, 600, 14, 8, 0xff2a1a, s.audio_led[3]);   // Source Monitor
    for (int i = 0; i < 8; i++) {
        int r = p.ind[i] % 3, yl = p.ind[8 + i] % 3;
        // blinking LEDs are lit while the blinking annunciators on the display are dark (as on the ASR)
        bool ron = r == 1 || (r == 2 && !blink), yon = yl == 1 || (yl == 2 && !blink);
        led(765.5f + 61.6f * i, 512, 14, 8, yon ? 0xffc020 : 0xff2a1a, ron || yon);
    }
    // bracket from the last track LED to its legend, and the lines around "Mode"
    line(1206, 512, 1230, 512, 0x9a9ca0); line(1230, 512, 1230, 494, 0x9a9ca0); line(1230, 494, 1236, 494, 0x9a9ca0);
    // arrow triangles (outlines) around the cursor buttons
    auto tri = [&](float x1, float y1, float x2, float y2, float x3, float y3) {
        SDL_Color c{ 0xd2, 0xd3, 0xd5, 255 }, b{ 0x24, 0x25, 0x27, 255 };
        float cx = (x1 + x2 + x3) / 3, cy = (y1 + y2 + y3) / 3, sh = 0.62f;
        SDL_Vertex v[6] = {
            { { X(x1), Y(y1) }, c, {0, 0} }, { { X(x2), Y(y2) }, c, {0, 0} }, { { X(x3), Y(y3) }, c, {0, 0} },
            { { X(cx + (x1 - cx) * sh), Y(cy + (y1 - cy) * sh) }, b, {0, 0} },
            { { X(cx + (x2 - cx) * sh), Y(cy + (y2 - cy) * sh) }, b, {0, 0} },
            { { X(cx + (x3 - cx) * sh), Y(cy + (y3 - cy) * sh) }, b, {0, 0} } };
        SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_NONE);
        SDL_RenderGeometry(R, nullptr, v, 3, nullptr, 0);
        SDL_RenderGeometry(R, nullptr, v + 3, 3, nullptr, 0);
    };
    tri(1439, 342, 1431, 355, 1447, 355);      // up
    tri(1439, 478, 1431, 465, 1447, 465);      // down
    tri(1371, 418, 1384, 410, 1384, 426);      // left
    tri(1507, 418, 1494, 410, 1494, 426);      // right
    fill(rect(731, 395, 70, 1.6f), LEGEND_COLOUR[2]); fill(rect(852, 395, 68, 1.6f), LEGEND_COLOUR[2]);

    // ---- display window: smoked glass with the VFD behind it
    {
        SDL_FRect w = rect(668, 228, 619, 149);
        fill({ w.x - K * 2, w.y - K * 2, w.w + K * 4, w.h + K * 4 }, 0x2b2c2f);
        fill(w, 0x030405);
        float vs = (619 - 34) * K / VfdRenderer::WIDTH;
        float vh = VfdRenderer::HEIGHT * vs;
        vfd.draw(p, w.x + 17 * K, w.y + (w.h - vh) / 2, s.ticks, vs, false);
        // reflection on the glass
        SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
        for (int i = 0; i < 12; i++) {
            SDL_SetRenderDrawColor(R, 255, 255, 255, (Uint8)(10 - i * 0.8f));
            SDL_FRect h{ w.x, w.y + i * w.h / 24, w.w, w.h / 24 };
            SDL_RenderFillRectF(R, &h);
        }
    }

    // ---- data entry slider
    slider_knob(DATA_X, DATA_TOP, DATA_BOT, s.data);

    // ---- floppy drive (mounted vertically): bezel, slot recess with the disk opening,
    //      eject button, label recess and the activity LED
    {
        fill(rect(1693, 213, 107, 399), 0x2c2d31);
        fill(rect(1695, 215, 103, 395), 0x070708);
        SDL_FRect face = rect(1700, 220, 93, 386);
        vgrad(face, 0x4a4b50, 0x424347);
        // slot recess (left), with a lighter bevel on its left edge
        fill(rect(1707, 227, 42, 369), 0x2a2b2e);
        vgrad(rect(1709, 230, 38, 363), 0x3a3b3f, 0x343539);
        fill(rect(1708, 232, 3, 100), 0x5a5b60); fill(rect(1708, 500, 3, 90), 0x5a5b60);
        fill(rect(1708, 592, 40, 3), 0x55565b);
        // the disk opening
        fill(rect(1719, 236, 21, 352), 0x1d1e20);
        fill(rect(1721, 238, 17, 348), 0x0b0b0c);
        if (s.disk) {
            vgrad(rect(1723, 241, 13, 344), 0x8f9196, 0x6c6e72);      // edge of the inserted disk
            fill(rect(1723, 241, 2, 344), 0xb0b2b6);
        }
        // eject button
        fill(rect(1761, 272, 25, 61), 0x1c1d1f);
        vgrad(rect(1763, 274, 21, 57), 0x34353a, 0x26272b);
        fill(rect(1763, 274, 21, 2), 0x55565b);
        // label recess
        fill(rect(1750, 345, 34, 134), 0x2a2b2e);
        fill(rect(1752, 347, 30, 130), 0x2f3034);
        // activity LED in its slot
        fill(rect(1766, 505, 14, 30), 0x2a2b2e);
        led(1773, 520, 6, 20, 0xff3a1a, s.drive_busy);
    }

    // ---- legends
    int i = 0;
    for (const PanelLegend &l : PANEL_LEGENDS) {
        SDL_FRect d{ X(l.x), Y(l.y), l.w * K, l.h * K };
        SDL_RenderCopyF(R, legend[i++], nullptr, &d);
    }
    (void)BG;
}
