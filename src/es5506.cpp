// Ensoniq ES5506 "OTTO" - see es5506.h for attribution.
#include <cstdlib>
#include "es5506.h"

namespace {
enum : uint32_t {
    CONTROL_BS1 = 0x8000, CONTROL_BS0 = 0x4000, CONTROL_CMPD = 0x2000,
    CONTROL_CA2 = 0x1000, CONTROL_CA1 = 0x0800, CONTROL_CA0 = 0x0400,
    CONTROL_LP4 = 0x0200, CONTROL_LP3 = 0x0100, CONTROL_IRQ = 0x0080,
    CONTROL_DIR = 0x0040, CONTROL_IRQE = 0x0020, CONTROL_BLE = 0x0010,
    CONTROL_LPE = 0x0008, CONTROL_LEI = 0x0004, CONTROL_STOP1 = 0x0002,
    CONTROL_STOP0 = 0x0001,
    CONTROL_LOOPMASK = CONTROL_BLE | CONTROL_LPE,
    CONTROL_STOPMASK = CONTROL_STOP1 | CONTROL_STOP0,
};
constexpr int FINE_FILTER_BIT = 16, FILTER_BIT = 12, FILTER_SHIFT = FINE_FILTER_BIT - FILTER_BIT;
constexpr int ULAW_MAXBITS = 8;
constexpr int VOLUME_BIT = 16;
constexpr int VOLUME_ACC_BIT = 20;
constexpr int ADDRESS_FRAC_BIT = 11;

inline int32_t sext18(uint32_t v) { return (int32_t)(v << 14) >> 14; }
inline int32_t apply_lowpass(int32_t out, int32_t cutoff, int32_t in) {
    return ((int32_t)(cutoff >> FILTER_SHIFT) * (out - in) / (1 << FILTER_BIT)) + in;
}
inline int32_t apply_highpass(int32_t out, int32_t cutoff, int32_t in, int32_t prev) {
    return out - prev + ((int32_t)(cutoff >> FILTER_SHIFT) * in) / (1 << (FILTER_BIT + 1)) + in / 2;
}
}

ES5506::ES5506() { compute_tables(); reset(); }

void ES5506::reset() {
    m_active_voices = 0x1f;
    m_mode = 0x17;
    m_irqv = 0x80;
    m_page = 0;
    m_write_latch = m_read_latch = 0;
    for (int j = 0; j < 32; j++) {
        m_voice[j] = Voice{};
        m_voice[j].index = j;
        m_voice[j].control = CONTROL_STOPMASK;
        m_voice[j].lvol = 1 << (VOLUME_BIT - 1);
        m_voice[j].rvol = 1 << (VOLUME_BIT - 1);
    }
}

void ES5506::compute_tables() {
    m_ulaw.resize(1 << ULAW_MAXBITS);
    for (int i = 0; i < (1 << ULAW_MAXBITS); i++) {
        uint16_t rawval = (i << (16 - ULAW_MAXBITS)) | (1 << (15 - ULAW_MAXBITS));
        uint8_t exponent = rawval >> 13;
        uint32_t mantissa = (rawval << 3) & 0xffff;
        if (exponent == 0) m_ulaw[i] = (int16_t)mantissa >> 7;
        else {
            mantissa = (mantissa >> 1) | (~mantissa & 0x8000);
            m_ulaw[i] = (int16_t)mantissa >> (7 - exponent);
        }
    }
    const int exponent_bit = 4, mantissa_bit = 8;
    const int volume_bit = exponent_bit + mantissa_bit;
    m_volume_shift = VOLUME_BIT - volume_bit;
    const uint32_t volume_len = 1u << volume_bit;
    m_volume_lookup.resize(volume_len);
    const uint32_t exponent_shift = 1u << exponent_bit;
    const uint32_t exponent_mask = exponent_shift - 1;
    const uint32_t mantissa_len = 1u << mantissa_bit;
    const uint32_t mantissa_mask = mantissa_len - 1;
    const uint32_t mantissa_shift = exponent_shift - mantissa_bit - 1;
    for (uint32_t i = 0; i < volume_len; i++) {
        uint32_t exponent = (i >> mantissa_bit) & exponent_mask;
        uint32_t mantissa = (i & mantissa_mask) | mantissa_len;
        m_volume_lookup[i] = (mantissa << mantissa_shift) >> (exponent_shift - exponent);
    }
    m_volume_acc_shift = (16 + exponent_mask) - VOLUME_ACC_BIT;
    // 21-bit integer, 11-bit fraction
    m_acc_mask = ((((uint64_t)1 << 21) - 1) << 11) | ((1u << 11) - 1);
}

int32_t ES5506::interpolate(int32_t s1, int32_t s2, uint64_t accum) const {
    const uint32_t shifted = 1u << ADDRESS_FRAC_BIT;
    const uint32_t mask = shifted - 1;
    accum &= mask & m_acc_mask;
    return (s1 * (int32_t)(shifted - accum) + s2 * (int32_t)accum) >> ADDRESS_FRAC_BIT;
}

void ES5506::apply_filters(Voice &v, int32_t &sample) {
    sample = apply_lowpass(sample, v.k1, v.o1n1); v.o1n1 = sample;
    sample = apply_lowpass(sample, v.k1, v.o2n1); v.o2n2 = v.o2n1; v.o2n1 = sample;
    switch ((v.control >> 8) & 3) {
    case 0:
        sample = apply_highpass(sample, v.k2, v.o3n1, v.o2n2); v.o3n2 = v.o3n1; v.o3n1 = sample;
        sample = apply_highpass(sample, v.k2, v.o4n1, v.o3n2); v.o4n1 = sample;
        break;
    case 1: // LP3
        sample = apply_lowpass(sample, v.k1, v.o3n1); v.o3n2 = v.o3n1; v.o3n1 = sample;
        sample = apply_highpass(sample, v.k2, v.o4n1, v.o3n2); v.o4n1 = sample;
        break;
    case 2: // LP4
        sample = apply_lowpass(sample, v.k2, v.o3n1); v.o3n2 = v.o3n1; v.o3n1 = sample;
        sample = apply_lowpass(sample, v.k2, v.o4n1); v.o4n1 = sample;
        break;
    case 3:
        sample = apply_lowpass(sample, v.k1, v.o3n1); v.o3n2 = v.o3n1; v.o3n1 = sample;
        sample = apply_lowpass(sample, v.k2, v.o4n1); v.o4n1 = sample;
        break;
    }
}

void ES5506::update_envelopes(Voice &v) {
    const uint32_t volume_max = (1u << VOLUME_BIT) - 1;
    v.ecount--;
    if (v.lvramp) {
        v.lvol += (int8_t)v.lvramp;
        if ((int32_t)v.lvol < 0) v.lvol = 0; else if (v.lvol > volume_max) v.lvol = volume_max;
    }
    if (v.rvramp) {
        v.rvol += (int8_t)v.rvramp;
        if ((int32_t)v.rvol < 0) v.rvol = 0; else if (v.rvol > volume_max) v.rvol = volume_max;
    }
    if (v.k1ramp && ((int32_t)v.k1ramp >= 0 || !(v.filtcount & 7))) {
        v.k1 += (int8_t)v.k1ramp;
        if ((int32_t)v.k1 < 0) v.k1 = 0; else if (v.k1 > 0xffff) v.k1 = 0xffff;
    }
    if (v.k2ramp && ((int32_t)v.k2ramp >= 0 || !(v.filtcount & 7))) {
        v.k2 += (int8_t)v.k2ramp;
        if ((int32_t)v.k2 < 0) v.k2 = 0; else if (v.k2 > 0xffff) v.k2 = 0xffff;
    }
    v.filtcount++;
}

void ES5506::check_for_end_forward(Voice &v, uint64_t &accum) {
    if (accum > v.end && !(v.control & CONTROL_LEI)) {
        if (v.control & CONTROL_IRQE) v.control |= CONTROL_IRQ;
        switch (v.control & CONTROL_LOOPMASK) {
        case 0: v.control |= CONTROL_STOP0; break;
        case CONTROL_LPE: accum = (v.start + (accum - v.end)) & m_acc_mask; break;
        case CONTROL_BLE:
            accum = (v.start + (accum - v.end)) & m_acc_mask;
            v.control = (v.control & ~CONTROL_LOOPMASK) | CONTROL_LEI;
            break;
        case CONTROL_LPE | CONTROL_BLE:
            accum = (v.end - (accum - v.end)) & m_acc_mask;
            v.control ^= CONTROL_DIR;
            break;
        }
    }
}

void ES5506::check_for_end_reverse(Voice &v, uint64_t &accum) {
    if (accum < v.start && !(v.control & CONTROL_LEI)) {
        if (v.control & CONTROL_IRQE) v.control |= CONTROL_IRQ;
        switch (v.control & CONTROL_LOOPMASK) {
        case 0: v.control |= CONTROL_STOP0; break;
        case CONTROL_LPE: accum = (v.end - (v.start - accum)) & m_acc_mask; break;
        case CONTROL_BLE:
            accum = (v.end - (v.start - accum)) & m_acc_mask;
            v.control = (v.control & ~CONTROL_LOOPMASK) | CONTROL_LEI;
            break;
        case CONTROL_LPE | CONTROL_BLE:
            accum = (v.start + (v.start - accum)) & m_acc_mask;
            v.control ^= CONTROL_DIR;
            break;
        }
    }
}

void ES5506::generate_voice(Voice &v, int32_t *dest) {
    const uint32_t freqcount = (uint32_t)v.freqcount;
    uint64_t accum = v.accum & m_acc_mask;
    if (!(v.control & CONTROL_STOPMASK)) {
        int bank = (v.control >> 14) & 3;
        int32_t s1 = read_sample(bank, (uint32_t)integer_addr(accum));
        int32_t s2 = read_sample(bank, (uint32_t)integer_addr(accum, 1));
        if (v.control & CONTROL_CMPD) {
            s1 = m_ulaw[(uint16_t)s1 >> (16 - ULAW_MAXBITS)];
            s2 = m_ulaw[(uint16_t)s2 >> (16 - ULAW_MAXBITS)];
        } else {
            s1 = (int16_t)s1; s2 = (int16_t)s2;
        }
        s1 = interpolate(s1, s2, accum);
        if (!(v.control & CONTROL_DIR)) accum = (accum + freqcount) & m_acc_mask;
        else accum = (accum - freqcount) & m_acc_mask;
        apply_filters(v, s1);
        if (v.ecount != 0) update_envelopes(v);
        dest[0] += (int32_t)get_sample(s1, v.lvol);
        dest[1] += (int32_t)get_sample(s1, v.rvol);
        if (!(v.control & CONTROL_DIR)) check_for_end_forward(v, accum);
        else check_for_end_reverse(v, accum);
    } else {
        if (v.ecount != 0) update_envelopes(v);
    }
    v.accum = accum;
}

void ES5506::generate_irq(Voice &v, int n) {
    if (v.control & CONTROL_IRQ) {
        if (m_irqv & 0x80) {
            m_irqv = n & 0x1f;
            v.control &= ~CONTROL_IRQ;
            if (irq_cb) irq_cb(true);
        }
    }
}

void ES5506::generate_frame(int32_t out[12]) {
    for (int i = 0; i < 12; i++) out[i] = 0;
    for (int n = 0; n <= m_active_voices; n++) {
        Voice &v = m_voice[n];
        // (MAME stops a voice whose START equals END; the ASR-10 OS passes through START == END
        // while it moves the loop of a Transwave, so doing that here cut notes off)
        int ch = (v.control >> 10) & 7;
        if (ch > 5) ch = ch % 6;
        cur_voice = (int)(&v - m_voice);
        generate_voice(v, &out[ch * 2]);
        generate_irq(v, n);
    }
}

// ------------------------------------------------------------------ registers
void ES5506::reg_write_low(Voice &v, int reg, uint32_t data) {
    switch (reg) {
    case 0: v.control = data & 0xffff; break;
    case 1: v.freqcount = data & 0x1ffff; break;
    case 2: v.lvol = data & 0xffff; break;
    case 3: v.lvramp = (data & 0xff00) >> 8; break;
    case 4: v.rvol = data & 0xffff; break;
    case 5: v.rvramp = (data & 0xff00) >> 8; break;
    case 6: v.ecount = data & 0x1ff; v.filtcount = 0; break;
    case 7: v.k2 = data & 0xffff; break;
    case 8: v.k2ramp = ((data & 0xff00) >> 8) | ((data & 1) << 31); break;
    case 9: v.k1 = data & 0xffff; break;
    case 10: v.k1ramp = ((data & 0xff00) >> 8) | ((data & 1) << 31); break;
    case 11: m_active_voices = data & 0x1f; break;
    case 12: m_mode = data & 0x1f; break;
    case 15: m_page = data & 0x7f; break;
    }
}

void ES5506::reg_write_high(Voice &v, int reg, uint32_t data) {
    switch (reg) {
    case 0: v.control = data & 0xffff; break;
    case 1: v.start = data & 0xfffff800; break;
    case 2: v.end = data & 0xffffff80; break;
    case 3: v.accum = data; break;
    case 4: v.o4n1 = sext18(data); break;
    case 5: v.o3n1 = sext18(data); break;
    case 6: v.o3n2 = sext18(data); break;
    case 7: v.o2n1 = sext18(data); break;
    case 8: v.o2n2 = sext18(data); break;
    case 9: v.o1n1 = sext18(data); break;
    case 10: m_wst = data & 0x7f; break;
    case 11: m_wend = data & 0x7f; break;
    case 12: m_lrend = data & 0x7f; break;
    case 15: m_page = data & 0x7f; break;
    }
}

void ES5506::write(int offset, uint8_t data) {
    Voice &v = m_voice[m_page & 0x1f];
    int shift = 8 * (offset & 3);
    m_write_latch = (m_write_latch & ~(0xff000000u >> shift)) | ((uint32_t)data << (24 - shift));
    if (shift != 24) return;
    int reg = (offset >> 2) & 15;
    if (m_page < 0x20) reg_write_low(v, reg, m_write_latch);
    else if (m_page < 0x40) reg_write_high(v, reg, m_write_latch);
    else if (reg == 15) m_page = m_write_latch & 0x7f;
    m_write_latch = 0;
}

uint32_t ES5506::reg_read_low(Voice &v, int reg) {
    switch (reg) {
    case 0: return v.control;
    case 1: return (uint32_t)v.freqcount;
    case 2: return v.lvol;
    case 3: return v.lvramp << 8;
    case 4: return v.rvol;
    case 5: return v.rvramp << 8;
    case 6: return v.ecount;
    case 7: return v.k2;
    case 8: return (v.k2ramp << 8) | (v.k2ramp >> 31);
    case 9: return v.k1;
    case 10: return (v.k1ramp << 8) | (v.k1ramp >> 31);
    case 11: return m_active_voices;
    case 12: return m_mode;
    case 13: return read_pot ? (read_pot() & 0x3ff) : 0;
    case 14: { uint32_t r = m_irqv; m_irqv = 0x80; if (irq_cb) irq_cb(false); return r; }
    case 15: return m_page;
    }
    return 0;
}

uint32_t ES5506::reg_read_high(Voice &v, int reg) {
    switch (reg) {
    case 0: return v.control;
    case 1: return (uint32_t)v.start;
    case 2: return (uint32_t)v.end;
    case 3: return (uint32_t)v.accum;
    case 4: return v.o4n1 & 0x3ffff;
    case 5: return v.o3n1 & 0x3ffff;
    case 6: return v.o3n2 & 0x3ffff;
    case 7: return v.o2n1 & 0x3ffff;
    case 8: return v.o2n2 & 0x3ffff;
    case 9: return v.o1n1 & 0x3ffff;
    case 10: return m_wst;
    case 11: return m_wend;
    case 12: return m_lrend;
    case 13: return read_pot ? (read_pot() & 0x3ff) : 0;
    case 14: { uint32_t r = m_irqv; m_irqv = 0x80; if (irq_cb) irq_cb(false); return r; }
    case 15: return m_page;
    }
    return 0;
}

uint32_t ES5506::reg_read_test(Voice &, int reg) {
    switch (reg) {
    case 13: return read_pot ? (read_pot() & 0x3ff) : 0;
    case 14: return m_irqv;
    case 15: return m_page;
    }
    return 0;
}

uint8_t ES5506::read(int offset) {
    Voice &v = m_voice[m_page & 0x1f];
    int shift = 8 * (offset & 3);
    if (shift != 0) return m_read_latch >> (24 - shift);
    int reg = (offset >> 2) & 15;
    if (m_page < 0x20) m_read_latch = reg_read_low(v, reg);
    else if (m_page < 0x40) m_read_latch = reg_read_high(v, reg);
    else m_read_latch = reg_read_test(v, reg);
    return m_read_latch >> 24;
}

void ES5506::dump(FILE *f) const {
    fprintf(f, "OTTO active=%d mode=%02x page=%02x irqv=%02x\n", m_active_voices, m_mode, m_page, m_irqv);
    for (int n = 0; n < 32; n++) {
        const Voice &v = m_voice[n];
        if ((v.control & 3) && v.lvol == 0 && v.rvol == 0) continue;
        fprintf(f, " v%02d cr=%04x ca=%d bank=%d fc=%05x start=%08x end=%08x acc=%08x lv=%04x rv=%04x k1=%04x k2=%04x ec=%03x\n",
            n, v.control, (v.control >> 10) & 7, (v.control >> 14) & 3, (unsigned)v.freqcount, (unsigned)v.start, (unsigned)v.end, (unsigned)v.accum, v.lvol, v.rvol, v.k1, v.k2, v.ecount);
    }
}
