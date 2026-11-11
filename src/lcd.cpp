// Purple graphic LCD - see lcd.h.
#include "lcd.h"
#include "panel.h"
#include "vfd_art.h"
#include "lcd_font.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace {

constexpr uint32_t TEXT = 0xeceab8;                 // lit pixels (pale yellow-green)
constexpr uint32_t BACK_TL = 0x8d66ee, BACK_TR = 0xab6cf3, BACK_BL = 0x6e6ae7, BACK_BR = 0x9b66ee;

// The module is a 240 x 58 pixel graphic LCD with square pixels (measured on photos of it).
// The text line is 22 cells of 8 x 14 pixels: the 5x7 character set drawn two pixels thick
// (every dot is two pixels wide and two tall), 6 x 14 pixels of glyph and a 2 pixel gap.
// The legends use the same character set at 1 pixel thickness, 8 pixels per letter.
constexpr int PX_W = 240, PX_H = 58;
constexpr float CELL = LcdRenderer::WIDTH / PX_W;       // logical px per LCD pixel
constexpr int TEXT_X = 31, TEXT_Y = 34;                 // first text cell
constexpr int LEG_LEFT = 5, LEG_RIGHT = 235;           // the mode box's left edge; right edge of the right-hand legends
constexpr int LEG_ROW0 = 3, LEG_STEP = 11;              // legend rows
constexpr int DU = 4;                                   // texels per LCD pixel in the textures

void colour(SDL_Renderer *R, uint32_t c, uint8_t a = 255) { SDL_SetRenderDrawColor(R, c >> 16, (c >> 8) & 255, c & 255, a); }

// legend letters: 6 rows (the first letter of a word is a tall capital, the others small ones, one row
// lower), cell column 0..7; word cell i starts at pixel 8 * i
bool px_leg(const char *word, int i, int c, int r) {
    unsigned char ch = (unsigned char)word[i];
    if (ch < 'A' || ch > 'Z' || c < 0 || c >= 8 || r < 0 || r >= 6) return false;
    return ((i == 0 ? LCD_LEG_CAP : LCD_LEG_SMALL)[ch - 'A'][r] >> c & 1);
}
// the text line's pixels: an 8 x 14 cell copied from the module (lcd_font.h)
bool px_big(int ch, int c, int r) { return c >= 0 && c < 8 && r >= 0 && r < 14 && (LCD_BIG[ch][r] >> c & 1); }

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
    for (auto *t : cur) if (t) SDL_DestroyTexture(t);
    for (auto *t : legc) if (t) SDL_DestroyTexture(t);
    for (auto *t : legs) if (t) SDL_DestroyTexture(t);
    for (auto *t : inv) if (t) SDL_DestroyTexture(t);
    for (auto *t : big_g) if (t) SDL_DestroyTexture(t);
    for (auto *t : cur_g) if (t) SDL_DestroyTexture(t);
    for (auto *t : legc_g) if (t) SDL_DestroyTexture(t);
    for (auto *t : legs_g) if (t) SDL_DestroyTexture(t);
    for (auto *t : inv_g) if (t) SDL_DestroyTexture(t);
    for (SDL_Texture *t : { meter, meter_g }) if (t) SDL_DestroyTexture(t);
}

bool LcdRenderer::init(SDL_Renderer *r) {
    R = r;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    for (int ch = 0; ch < 64; ch++) {
        auto fb = [ch](int c, int rr) { return px_big(ch, c, rr); };
        big[ch] = dots_texture(R, 8, 14, fb); big_g[ch] = dots_texture(R, 8, 14, fb, true);
        // the selected field is shown inverted: the whole 8 x 16 cell lit, the character cut out
        auto fc = [fb](int c, int rr) { return rr == 0 || rr == 15 || !fb(c, rr - 1); };
        cur[ch] = dots_texture(R, 8, 16, fc); cur_g[ch] = dots_texture(R, 8, 16, fc, true);
        if (!big[ch] || !big_g[ch] || !cur[ch] || !cur_g[ch]) return false;
    }
    for (int k = 0; k < 26; k++) {
        auto fcap = [k](int c, int rr) { return rr >= 0 && rr < 6 && c >= 0 && c < 8 && (LCD_LEG_CAP[k][rr] >> c & 1); };
        auto fsm = [k](int c, int rr) { return rr >= 0 && rr < 6 && c >= 0 && c < 8 && (LCD_LEG_SMALL[k][rr] >> c & 1); };
        legc[k] = dots_texture(R, 8, 6, fcap); legc_g[k] = dots_texture(R, 8, 6, fcap, true);
        legs[k] = dots_texture(R, 8, 6, fsm); legs_g[k] = dots_texture(R, 8, 6, fsm, true);
        if (!legc[k] || !legs[k] || !legc_g[k] || !legs_g[k]) return false;
    }
    auto fm = [](int c, int) { return c >= 2 && c <= 5; };
    meter = dots_texture(R, 8, 14, fm); meter_g = dots_texture(R, 8, 14, fm, true);
    const int n = (int)(sizeof(VFD_LEGENDS) / sizeof(VFD_LEGENDS[0]));
    for (int i = 0; i < n && i < 32; i++) {            // the inverted legends: a lit box, the letters cut out
        const char *t = VFD_LEGENDS[i].text; const int len = (int)strlen(t);
        auto fi = [t](int c, int rr) {                    // box: 8 pixels per letter, 1 pixel border all round
            if (c == 0 || rr == 0 || rr == 7) return true;
            int ch = (c - 1) / 8, gx = (c - 1) % 8;
            return !px_leg(t, ch, gx, rr - 1);
        };
        inv[i] = dots_texture(R, len * 8, 8, fi); inv_g[i] = dots_texture(R, len * 8, 8, fi, true);
        if (!inv[i] || !inv_g[i]) return false;
    }
    return true;
}

namespace {
// Every legend has one fixed place on the screen (the module never moves them). Legends that
// share a place are never lit together, so the first one that is lit is drawn.
struct Slot { int col, row; bool right; };
Slot slot_of(const char *t) {
    auto is = [&](std::initializer_list<const char *> l) { for (const char *n : l) if (!strcmp(t, n)) return true; return false; };
    // left: INST / BANK, SYSTEM / MACRO, SEQ / SONG ... share one column, the second row directly under the first
    if (is({ "INST", "MIDI", "SYSTEM", "LAYER", "SEQ" })) return { 64, 0, false };
    if (is({ "SONG", "MACRO", "BANK" })) return { 64, 1, false };
    if (is({ "PITCH", "FILTER", "AMP", "ENV" })) return { 120, 0, false };
    if (is({ "LFO", "WAVE" })) return { 120, 1, false };
    if (is({ "ODUB", "REC", "PLAY", "STOP" })) return { LEG_RIGHT, 0, true };
    if (is({ "TRACK", "BAR", "BEAT", "CLOCK" })) return { LEG_RIGHT, 1, true };
    if (is({ "STEP" })) return { 147, 0, false };
    return { 147, 1, false };                                 // REP, under STEP
}
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
    int mode = -1, mode_st = 0;
    const int n = (int)(sizeof(VFD_LEGENDS) / sizeof(VFD_LEGENDS[0]));
    struct Placed { int idx; Slot sl; int st; };
    std::vector<Placed> placed;
    for (int i = 0; i < n; i++) {
        const VfdLegend &l = VFD_LEGENDS[i];
        int st = (l.group == 0 ? p.ind2 : p.ind3)[l.index & 15] % 3;
        if (!st) continue;
        bool first_col = l.group == 0 && (i % 5) == 0;      // LOAD, CMD, EDIT
        if (first_col) { mode = i; mode_st = st; continue; }
        Slot sl = slot_of(l.text);
        bool taken = false;
        for (const Placed &q : placed) if (q.sl.col == sl.col && q.sl.row == sl.row && q.sl.right == sl.right) taken = true;
        if (!taken) placed.push_back({ i, sl, st });
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
    auto legend_w = [&](const char *t) { return (float)(strlen(t) * 8) * u; };
    auto draw_legend = [&](int idx, float px, int row, bool inverted, bool letters) {   // px = left edge in LCD pixels
        const char *t = VFD_LEGENDS[idx].text;
        const float lx = x + px * u, ly = y + (float)(LEG_ROW0 + LEG_STEP * row) * u;
        if (inverted) blit(inv[idx], inv_g[idx], SDL_FRect{ lx, ly, legend_w(t), 8 * u });
        if (!inverted || !letters) {                         // (an inverted box without its letters: they are filled in)
            for (size_t i = 0; t[i]; i++) {
                int k = t[i] - 'A'; if (k < 0 || k >= 26) continue;
                SDL_FRect d{ lx + (float)(1 + i * 8) * u, ly + u, 8 * u, 6 * u };
                blit(i == 0 ? legc[k] : legs[k], i == 0 ? legc_g[k] : legs_g[k], d);
            }
        }
    };
    // the mode box never moves or goes out; only its letters blink (cut out when on, filled in when off)
    if (mode >= 0) draw_legend(mode, (float)LEG_LEFT, 0, true, mode_st == 1 || blink);
    for (const Placed &q : placed) {
        if (!(q.st == 1 || blink)) continue;
        float px = (float)q.sl.col;
        if (q.sl.right) px -= legend_w(VFD_LEGENDS[q.idx].text) / u;
        draw_legend(q.idx, px, q.sl.row, false, true);
    }

    // ---- the text line
    for (int i = 0; i < Panel::COLS; i++) {
        const float cx = x + (float)(TEXT_X + 8 * i) * u, cy = y + (float)TEXT_Y * u;
        unsigned char ch = (unsigned char)p.disp[i];
        SDL_Texture *tx = nullptr, *tg = nullptr;
        const int gi = (ch >= 0x20 && ch < 0x60) ? ch - 0x20 : 0;
        if (p.attr[i] & 3) {                                                // selected field / edit cursor: inverted
            SDL_FRect d{ cx, cy - u, 8 * u, 16 * u };
            blit(cur[gi], cur_g[gi], d);
            continue;
        }
        if (ch == ' ' && i < p.meter) { tx = meter; tg = meter_g; }          // level meter: a lit bar
        else if (ch > ' ' && ch < 0x60) { tx = big[ch - 0x20]; tg = big_g[ch - 0x20]; }
        if (tx) blit(tx, tg, SDL_FRect{ cx, cy, 8 * u, 14 * u });
    }
    SDL_SetRenderDrawBlendMode(R, old);
}
