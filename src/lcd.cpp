// Purple graphic LCD - see lcd.h.
#include "lcd.h"
#include "panel.h"
#include "vfd_art.h"
#include <algorithm>
#include <cmath>
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

constexpr uint32_t TEXT = 0xeceab8;                 // lit pixels (pale yellow-green)
constexpr uint32_t BACK_TL = 0x8d66ee, BACK_TR = 0xab6cf3, BACK_BL = 0x6e6ae7, BACK_BR = 0x9b66ee;

// The module is a 240 x 58 pixel graphic LCD with square pixels (measured on photos of it).
// The text line is 22 cells of 8 x 14 pixels: the 5x7 character set drawn two pixels thick
// (every dot is two pixels wide and two tall), 6 x 14 pixels of glyph and a 2 pixel gap.
// The legends use the same character set at 1 pixel thickness, 8 pixels per letter.
constexpr int PX_W = 240, PX_H = 58;
constexpr float CELL = LcdRenderer::WIDTH / PX_W;       // logical px per LCD pixel
constexpr int TEXT_X = 32, TEXT_Y = 34;                 // first text cell
constexpr int LEG_LEFT = 5, LEG_MID = 64, LEG_RIGHT = 227, LEG_COL = 56;   // legend columns
constexpr int LEG_ROW0 = 3, LEG_STEP = 11;              // legend rows
constexpr int DU = 4;                                   // texels per LCD pixel in the textures

void colour(SDL_Renderer *R, uint32_t c, uint8_t a = 255) { SDL_SetRenderDrawColor(R, c >> 16, (c >> 8) & 255, c & 255, a); }

const uint8_t *glyph(unsigned char ch) { return (ch >= 0x20 && ch < 0x60) ? FONT5X7[ch - 0x20] : FONT5X7[0]; }
bool dot5x7(const uint8_t *g, int c, int r) { return c >= 0 && c < 5 && r >= 0 && r < 7 && (g[c] >> r & 1); }
// the text line's pixels: 6 x 14, each dot of the 5x7 glyph is 2 pixels wide and 2 tall
bool px_big(const uint8_t *g, int c, int r) { return dot5x7(g, c, r / 2) || dot5x7(g, c - 1, r / 2); }

// a w x h dot matrix as a white alpha texture, DU x DU texels per dot. grid = false: solid
// pixels; grid = true: only the thin gaps between the pixels (the dot grid you see on the module,
// drawn on top when the pixels are large enough on screen)
SDL_Texture *dots_texture(SDL_Renderer *R, int w, int h, const std::function<bool(int, int)> &on, bool grid = false) {
    const int tw = w * DU, th = h * DU;
    std::vector<uint32_t> px((size_t)tw * th, 0x00ffffff);
    for (int c = 0; c < w; c++) for (int r = 0; r < h; r++) {
        if (!on(c, r)) continue;
        for (int y = 0; y < DU; y++) for (int x = 0; x < DU; x++) {
            bool gap = x == DU - 1 || y == DU - 1;
            if (!grid || gap) px[(size_t)(r * DU + y) * tw + c * DU + x] = 0xffffffff;
        }
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
    for (auto *t : big_g) if (t) SDL_DestroyTexture(t);
    for (auto *t : small_g) if (t) SDL_DestroyTexture(t);
    for (auto *t : inv_g) if (t) SDL_DestroyTexture(t);
    for (SDL_Texture *t : { meter, underline, meter_g, underline_g }) if (t) SDL_DestroyTexture(t);
}

bool LcdRenderer::init(SDL_Renderer *r) {
    R = r;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    for (int ch = 0; ch < 64; ch++) {
        const uint8_t *g = FONT5X7[ch];
        auto fb = [&](int c, int rr) { return px_big(g, c, rr); };
        auto fs = [&](int c, int rr) { return dot5x7(g, c, rr); };
        big[ch] = dots_texture(R, 6, 14, fb); big_g[ch] = dots_texture(R, 6, 14, fb, true);
        small_[ch] = dots_texture(R, 5, 7, fs); small_g[ch] = dots_texture(R, 5, 7, fs, true);
        if (!big[ch] || !small_[ch] || !big_g[ch] || !small_g[ch]) return false;
    }
    auto fm = [](int c, int) { return c >= 1 && c <= 4; };
    auto fu = [](int, int) { return true; };
    meter = dots_texture(R, 6, 14, fm); meter_g = dots_texture(R, 6, 14, fm, true);
    underline = dots_texture(R, 6, 2, fu); underline_g = dots_texture(R, 6, 2, fu, true);
    const int n = (int)(sizeof(VFD_LEGENDS) / sizeof(VFD_LEGENDS[0]));
    for (int i = 0; i < n && i < 32; i++) {            // the inverted legends: a lit box, the letters cut out
        const char *t = VFD_LEGENDS[i].text; const int len = (int)strlen(t);
        auto fi = [&](int c, int rr) {
            int ch = (c - 1) / 8, gx = (c - 1) % 8, gy = rr - 1;
            if (c == 0) return true;
            return !(gx >= 0 && gx < 5 && gy >= 0 && gy < 7 && dot5x7(glyph((unsigned char)t[ch]), gx, gy));
        };
        inv[i] = dots_texture(R, len * 8 + 1, 9, fi); inv_g[i] = dots_texture(R, len * 8 + 1, 9, fi, true);
        if (!inv[i] || !inv_g[i]) return false;
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
    // other lit legends of the left group are stacked in two rows from pixel 64; those of the
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
    const float u = CELL * s;                               // one LCD pixel on screen
    // the pixel grid (gaps between the lit pixels) only shows when the pixels are big on the real screen
    float sx = 1, sy = 1; SDL_RenderGetScale(R, &sx, &sy);
    const float gridA = std::min(0.8f, std::max(0.0f, (u * sx - 2.2f) / 3.5f));
    auto blit = [&](SDL_Texture *solid, SDL_Texture *grid, const SDL_FRect &d) {
        tint(solid, TEXT); SDL_RenderCopyF(R, solid, nullptr, &d);
        if (gridA > 0.02f) {
            SDL_SetTextureColorMod(grid, 0x7a, 0x63, 0xe0); SDL_SetTextureAlphaMod(grid, (Uint8)(gridA * 255));
            SDL_RenderCopyF(R, grid, nullptr, &d);
        }
    };
    auto legend_w = [&](const char *t) { return (float)(strlen(t) * 8 + 1) * u; };
    auto draw_legend = [&](int idx, float px, int row, bool inverted) {   // px = left edge in LCD pixels
        const char *t = VFD_LEGENDS[idx].text;
        const float lx = x + px * u, ly = y + (float)(LEG_ROW0 + LEG_STEP * row) * u;
        if (inverted) {
            SDL_FRect d{ lx, ly, legend_w(t), 9 * u };
            blit(inv[idx], inv_g[idx], d);
        } else {
            for (size_t i = 0; t[i]; i++) {
                unsigned char ch = (unsigned char)t[i];
                int gi = (ch >= 0x20 && ch < 0x60) ? ch - 0x20 : 0;
                SDL_FRect d{ lx + (float)(1 + i * 8) * u, ly + u, 5 * u, 7 * u };
                blit(small_[gi], small_g[gi], d);
            }
        }
    };
    if (mode >= 0 && mode_on) draw_legend(mode, (float)LEG_LEFT, 0, true);
    for (size_t k = 0; k < left.size(); k++)
        if (left[k].on) draw_legend(left[k].idx, (float)(LEG_MID + LEG_COL * (int)(k / 2)), (int)(k % 2), false);
    for (size_t k = 0; k < right.size(); k++)
        if (right[k].on) {
            float w = legend_w(VFD_LEGENDS[right[k].idx].text) / u;
            draw_legend(right[k].idx, (float)(LEG_RIGHT - LEG_COL * (int)(k / 2)) - w, (int)(k % 2), false);
        }

    // ---- the text line
    for (int i = 0; i < Panel::COLS; i++) {
        const float cx = x + (float)(TEXT_X + 8 * i) * u, cy = y + (float)TEXT_Y * u;
        unsigned char ch = (unsigned char)p.disp[i];
        SDL_Texture *tx = nullptr, *tg = nullptr;
        if (ch == ' ' && i < p.meter) { tx = meter; tg = meter_g; }          // level meter: a lit bar
        else if (ch > ' ' && ch < 0x60) { tx = big[ch - 0x20]; tg = big_g[ch - 0x20]; }
        if (tx) blit(tx, tg, SDL_FRect{ cx, cy, 6 * u, 14 * u });
        if (p.attr[i] & 3) blit(underline, underline_g, SDL_FRect{ cx, cy + 16 * u, 6 * u, 2 * u });   // underline / edit cursor
    }
    SDL_SetRenderDrawBlendMode(R, old);
}
