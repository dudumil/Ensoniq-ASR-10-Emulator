// Purple graphic LCD - see lcd.h.
#include "lcd.h"
#include "panel.h"
#include "vfd_art.h"
#include <algorithm>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace {
static const uint8_t FONT5X7[64][5] = {   // codes $20-$5F, one byte per column (bit 0 = top row)
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40}
};

constexpr uint32_t TEXT = 0xeceab8;                 // lit dots (pale yellow-green)
constexpr uint32_t BACK_TL = 0x8d66ee, BACK_TR = 0xab6cf3, BACK_BL = 0x6e6ae7, BACK_BR = 0x9b66ee;

// the big text line: 22 cells of 5x7 dots, drawn bold (6x8 dots), slightly taller than wide
constexpr float PITCH = 26.0f, LEFT = 13.0f, TOP = 80.0f, DW = 3.5f, DH = 3.9f;
// legends: small dots (5x7 letters, 7 dots per letter cell)
constexpr float LU = 2.5f;
constexpr float ROW0 = 8.0f, ROW_STEP = 27.0f;
constexpr int DU = 8;                               // texels per dot in the textures

void colour(SDL_Renderer *R, uint32_t c, uint8_t a = 255) { SDL_SetRenderDrawColor(R, c >> 16, (c >> 8) & 255, c & 255, a); }

const uint8_t *glyph(unsigned char ch) { return (ch >= 0x20 && ch < 0x60) ? FONT5X7[ch - 0x20] : FONT5X7[0]; }
bool dot5x7(const uint8_t *g, int c, int r) { return c >= 0 && c < 5 && r >= 0 && r < 7 && (g[c] >> r & 1); }
// bold: every dot also lights its right and lower neighbours (6x8 dots, strokes two dots thick)
bool dot_bold(const uint8_t *g, int c, int r) {
    return dot5x7(g, c, r) || dot5x7(g, c - 1, r) || dot5x7(g, c, r - 1) || dot5x7(g, c - 1, r - 1);
}

// a w x h dot matrix as a white alpha texture, DU x DU texels per dot with a thin gap
SDL_Texture *dots_texture(SDL_Renderer *R, int w, int h, const std::function<bool(int, int)> &on) {
    const int tw = w * DU, th = h * DU;
    std::vector<uint32_t> px((size_t)tw * th, 0x00ffffff);
    for (int c = 0; c < w; c++) for (int r = 0; r < h; r++) {
        if (!on(c, r)) continue;
        for (int y = 0; y < DU - 1; y++) for (int x = 0; x < DU - 1; x++) px[(size_t)(r * DU + y) * tw + c * DU + x] = 0xffffffff;
    }
    SDL_Texture *t = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, tw, th);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, px.data(), tw * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

void tint(SDL_Texture *t, uint32_t c) {
    SDL_SetTextureColorMod(t, c >> 16, (c >> 8) & 255, c & 255);
}
}

LcdRenderer::~LcdRenderer() {
    for (auto *t : big) if (t) SDL_DestroyTexture(t);
    for (auto *t : small_) if (t) SDL_DestroyTexture(t);
    for (auto *t : inv) if (t) SDL_DestroyTexture(t);
    if (meter) SDL_DestroyTexture(meter);
    if (underline) SDL_DestroyTexture(underline);
}

bool LcdRenderer::init(SDL_Renderer *r) {
    R = r;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    for (int ch = 0; ch < 64; ch++) {
        const uint8_t *g = FONT5X7[ch];
        big[ch] = dots_texture(R, 6, 8, [&](int c, int rr) { return dot_bold(g, c, rr); });
        small_[ch] = dots_texture(R, 5, 7, [&](int c, int rr) { return dot5x7(g, c, rr); });
        if (!big[ch] || !small_[ch]) return false;
    }
    meter = dots_texture(R, 6, 8, [](int c, int) { return c >= 1 && c <= 4; });
    underline = dots_texture(R, 6, 1, [](int, int) { return true; });
    const int n = (int)(sizeof(VFD_LEGENDS) / sizeof(VFD_LEGENDS[0]));
    for (int i = 0; i < n && i < 32; i++) {            // the inverted legends: a lit box, the letters cut out
        const char *t = VFD_LEGENDS[i].text; const int len = (int)strlen(t);
        inv[i] = dots_texture(R, len * 7, 9, [&](int c, int rr) {
            int ch = c / 7, gx = c % 7 - 1, gy = rr - 1;
            return !(gx >= 0 && gx < 5 && gy >= 0 && gy < 7 && dot5x7(glyph((unsigned char)t[ch]), gx, gy));
        });
        if (!inv[i]) return false;
    }
    return true;
}

namespace {
struct Leg { int idx; bool on; };
}

void LcdRenderer::draw(const Panel &p, float x, float y, uint32_t ticks, float s) {
    if (!R) return;
    SDL_BlendMode old; SDL_GetRenderDrawBlendMode(R, &old);
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    const bool blink = (ticks / 250) & 1;

    // ---- backlight: a gradient (bluer at the bottom left, pinker at the top right)
    {
        auto col = [](uint32_t c) { return SDL_Color{ (Uint8)(c >> 16), (Uint8)(c >> 8), (Uint8)c, 255 }; };
        SDL_Vertex v[4] = {
            { { x, y }, col(BACK_TL), { 0, 0 } }, { { x + WIDTH * s, y }, col(BACK_TR), { 0, 0 } },
            { { x, y + HEIGHT * s }, col(BACK_BL), { 0, 0 } }, { { x + WIDTH * s, y + HEIGHT * s }, col(BACK_BR), { 0, 0 } } };
        const int idx[6] = { 0, 1, 2, 1, 3, 2 };
        SDL_RenderGeometry(R, nullptr, v, 4, idx, 6);
        colour(R, 0x4a3a9a, 120);
        SDL_FRect b{ x, y, WIDTH * s, HEIGHT * s }; SDL_RenderDrawRectF(R, &b);
    }

    // ---- annunciators: which legends are lit, and where they go
    // The mode legend of the first column (LOAD / CMD / EDIT) is inverted at the top left. The
    // other lit legends of the left group are stacked in two rows from x = 150; those of the
    // right group (ENV ... STOP, AMP ... CLOCK) likewise from the right edge.
    std::vector<Leg> left, right;
    int mode = -1; bool mode_on = false;
    const int n = (int)(sizeof(VFD_LEGENDS) / sizeof(VFD_LEGENDS[0]));
    for (int i = 0; i < n; i++) {
        const VfdLegend &l = VFD_LEGENDS[i];
        int st = (l.group == 0 ? p.ind2 : p.ind3)[l.index & 15] % 3;
        if (!st) continue;
        bool on = st == 1 || blink;
        bool first_col = l.group == 0 && (i % 5) == 0;      // LOAD, CMD, EDIT
        if (first_col) { mode = i; mode_on = on; }
        else (l.group == 0 ? left : right).push_back({ i, on });
    }
    const float u = LU * s;
    auto legend_w = [&](const char *t) { return (float)strlen(t) * 7 * u; };
    auto draw_legend = [&](int idx, float lx, float ly, bool inverted) {
        const char *t = VFD_LEGENDS[idx].text;
        if (inverted) {
            SDL_FRect d{ lx, ly, legend_w(t), 9 * u };
            tint(inv[idx], TEXT); SDL_RenderCopyF(R, inv[idx], nullptr, &d);
        } else {
            for (size_t i = 0; t[i]; i++) {
                unsigned char ch = (unsigned char)t[i];
                SDL_Texture *tx = small_[(ch >= 0x20 && ch < 0x60) ? ch - 0x20 : 0];
                SDL_FRect d{ lx + ((float)i * 7 + 1) * u, ly + u, 5 * u, 7 * u };
                tint(tx, TEXT); SDL_RenderCopyF(R, tx, nullptr, &d);
            }
        }
    };
    if (mode >= 0 && mode_on) draw_legend(mode, x + 10 * s, y + ROW0 * s, true);
    const float colw = 118 * s;
    for (size_t k = 0; k < left.size(); k++) {
        if (!left[k].on) continue;
        draw_legend(left[k].idx, x + 150 * s + (float)(k / 2) * colw, y + (ROW0 + ROW_STEP * (float)(k % 2)) * s, false);
    }
    for (size_t k = 0; k < right.size(); k++) {
        if (!right[k].on) continue;
        float lx = x + (WIDTH - 10) * s - (float)(k / 2) * colw - legend_w(VFD_LEGENDS[right[k].idx].text);
        draw_legend(right[k].idx, lx, y + (ROW0 + ROW_STEP * (float)(k % 2)) * s, false);
    }

    // ---- the text line
    for (int i = 0; i < Panel::COLS; i++) {
        const float cx = x + (LEFT + PITCH * i) * s, cy = y + TOP * s;
        unsigned char ch = (unsigned char)p.disp[i];
        SDL_Texture *tx = nullptr;
        if (ch == ' ' && i < p.meter) tx = meter;           // level meter: a lit bar
        else if (ch > ' ' && ch < 0x60) tx = big[ch - 0x20];
        if (tx) {
            SDL_FRect d{ cx, cy, 6 * DW * s, 8 * DH * s };
            tint(tx, TEXT); SDL_RenderCopyF(R, tx, nullptr, &d);
        }
        if (p.attr[i] & 3) {                                // underline / edit cursor
            SDL_FRect d{ cx, cy + (8 * DH + 1.0f) * s, 6 * DW * s, DH * 0.8f * s };
            tint(underline, TEXT); SDL_RenderCopyF(R, underline, nullptr, &d);
        }
    }
    SDL_SetRenderDrawBlendMode(R, old);
}
