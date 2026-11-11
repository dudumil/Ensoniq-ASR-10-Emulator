// Purple graphic LCD for the ASR-10 front end: the alternative to the VFD, as in the LCD
// replacement modules fitted to these units. Dot-matrix text on a purple backlight; the mode
// legend (LOAD / CMD / EDIT) is shown inverted, the other lit annunciators are collected in
// two small areas at the top.
#pragma once
#include <SDL.h>
#include <cstdint>

class Panel;

class LcdRenderer {
public:
    bool init(SDL_Renderer *r);                 // builds the dot-matrix textures
    // x,y = top-left of the screen; s = scale (1 = WIDTH x HEIGHT logical px)
    void draw(const Panel &p, float x, float y, uint32_t ticks, float s);
    static constexpr float WIDTH = 590, HEIGHT = 142.6f;   // 240 x 58 LCD pixels
    ~LcdRenderer();

private:
    SDL_Renderer *R = nullptr;
    SDL_Texture *big[64] = {}, *cur[64] = {}, *legc[26] = {}, *legs[26] = {}, *inv[32] = {}, *meter = nullptr;
    SDL_Texture *big_g[64] = {}, *cur_g[64] = {}, *legc_g[26] = {}, *legs_g[26] = {}, *inv_g[32] = {}, *meter_g = nullptr;   // the pixel grid
};
