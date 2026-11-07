// Ensoniq ES5506 "OTTO" wavetable voice generator.
// Algorithms derived from MAME's es5506.cpp (BSD-3-Clause, copyright Aaron Giles),
// restructured as a standalone class for the ASR-10 emulator.
#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <cstdio>

class ES5506 {
public:
    ES5506();
    void reset();

    // host interface: offset 0..63 (reg = offset>>2, byte = offset&3, big endian)
    uint8_t read(int offset);
    void    write(int offset, uint8_t data);

    // generate one output frame (one pass over all active voices).
    // out[ch*2+0] = left, out[ch*2+1] = right, 6 channels, 20-bit signed range
    void generate_frame(int32_t out[12]);

    int  active_voices() const { return m_active_voices; }
    double sample_rate(double clock) const { return clock / (16.0 * (m_active_voices + 1)); }

    std::function<uint16_t(int bank, uint32_t addr)> read_sample;
    int cur_voice = 0;                                             // voice being fetched (for read_sample)  // 16-bit word fetch
    std::function<uint16_t()> read_pot;                            // 10-bit POT A/D
    std::function<void(bool)> irq_cb;                              // IRQB asserted (true) / released

    bool irq_pending() const { return !(m_irqv & 0x80); }
    void dump(FILE *f) const;

private:
    struct Voice {
        uint32_t control = 0;
        uint64_t freqcount = 0, start = 0, end = 0, accum = 0;
        uint32_t lvol = 0, lvramp = 0, rvol = 0, rvramp = 0, ecount = 0;
        uint32_t k2 = 0, k2ramp = 0, k1 = 0, k1ramp = 0;
        int32_t  o4n1 = 0, o3n1 = 0, o3n2 = 0, o2n1 = 0, o2n2 = 0, o1n1 = 0;
        uint8_t  index = 0, filtcount = 0;
    };

    void compute_tables();
    int32_t interpolate(int32_t s1, int32_t s2, uint64_t accum) const;
    void apply_filters(Voice &v, int32_t &sample);
    void update_envelopes(Voice &v);
    void check_for_end_forward(Voice &v, uint64_t &accum);
    void check_for_end_reverse(Voice &v, uint64_t &accum);
    void generate_voice(Voice &v, int32_t *dest);
    void generate_irq(Voice &v, int n);

    void reg_write_low(Voice &v, int reg, uint32_t data);
    void reg_write_high(Voice &v, int reg, uint32_t data);
    uint32_t reg_read_low(Voice &v, int reg);
    uint32_t reg_read_high(Voice &v, int reg);
    uint32_t reg_read_test(Voice &v, int reg);

    uint64_t acc_shifted(uint64_t v) const { return v; }      // 21.11 fixed point, no extra shift
    uint64_t integer_addr(uint64_t accum, int bias = 0) const {
        return ((accum + ((uint64_t)bias << 11)) & m_acc_mask) >> 11;
    }
    uint64_t get_volume(uint32_t vol) const { return m_volume_lookup[vol >> m_volume_shift]; }
    int64_t  get_sample(int32_t s, uint32_t vol) const { return ((int64_t)s * (int64_t)get_volume(vol)) >> m_volume_acc_shift; }

    Voice    m_voice[32];
    uint64_t m_acc_mask;
    int      m_volume_shift;
    int64_t  m_volume_acc_shift;
    std::vector<int16_t>  m_ulaw;
    std::vector<uint32_t> m_volume_lookup;
    uint8_t  m_page = 0, m_active_voices = 31;
    uint16_t m_mode = 0x17;
    uint8_t  m_irqv = 0x80;
    uint32_t m_write_latch = 0, m_read_latch = 0;
    uint8_t  m_wst = 0, m_wend = 0, m_lrend = 0;
};
