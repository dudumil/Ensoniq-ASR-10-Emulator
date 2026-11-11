// Ensoniq ASR-10 machine glue.
//
// Memory map (from the boot ROM's MC68302 chip-select programming):
//   000000-xxxxxx  DRAM (OS + sample memory, shared with the OTTO), mirrored
//   F80000-FBFFFF  CS0  boot ROM (256 KB, read only)
//   FC2000-FC3FFF  CS2  ES5506 OTTO (8-bit, odd bytes, A6-A1 = register offset)
//   FC4000-FC5FFF  CS3  I/O: FC4000 uPD72069 FDC, FC4800 2681 DUART,
//                            FC5000 SCSI (WD33C93, optional), FC5800 FDC DMA port
//   FC6000-FC6FFF  MC68302 dual-port RAM + internal registers (BAR = $0FC6)
//   FF6000-FF7FFF  CS1  (write only, unknown)
#include "u8file.h"
#include "asr10.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <string>

// debug switches read once from the environment (one static per call site)
#define DBG(name) ([]() { static const bool f = getenv(#name) != nullptr; return f; }())
extern "C" {
#include "m68k.h"
}

Asr10 *g_machine = nullptr;

// --------------------------------------------------------------- Musashi glue
extern "C" {
unsigned int m68k_read_memory_8(unsigned int a) { return g_machine->read(a, 1); }
unsigned int m68k_read_memory_16(unsigned int a) { return g_machine->read(a, 2); }
unsigned int m68k_read_memory_32(unsigned int a) { return g_machine->read(a, 4); }
void m68k_write_memory_8(unsigned int a, unsigned int v) { g_machine->write(a, v, 1); }
void m68k_write_memory_16(unsigned int a, unsigned int v) { g_machine->write(a, v, 2); }
void m68k_write_memory_32(unsigned int a, unsigned int v) { g_machine->write(a, v, 4); }
unsigned int m68k_read_disassembler_8(unsigned int a) { return g_machine->read(a, 1); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return g_machine->read(a, 2); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return g_machine->read(a, 4); }
int asr_int_ack(int level) { return g_machine->imp.int_ack(level); }
void asr_reset_instr(void) {}
void asr_instr_hook(unsigned int pc) { if (g_machine->on_instr) g_machine->on_instr(pc); }
}

Asr10::Asr10() {
    g_machine = this;
    set_ram_mb(2);
    if (getenv("ASR_NOFX")) fx_enabled = false;
    if (const char *e = getenv("ASR_SLOT")) voice_slot_offset = atoi(e);
    if (const char *e = getenv("ASR_ROUTE")) sscanf(e, "%d,%d,%d,%d,%x", &esp_route[0], &esp_route[1], &esp_route[2], &esp_route[3], &direct_mask);
    imp.mem_read8 = [this](uint32_t a) { return (uint8_t)read(a, 1); };
    imp.mem_write8 = [this](uint32_t a, uint8_t v) { write(a, v, 1); };
    imp.irq_changed = [](int l) { m68k_set_irq(l); };
    imp.idma_dreq = [this](int) { return (fdc.dreq() || scsi.dreq()) ? 1 : 0; };
    imp.portb_in = [this]() -> uint16_t {
        uint16_t v = 0xffff;
        if (otto.irq_pending()) v &= ~(1 << 9);   // OTTO IRQB -> PB9 (active low)
        if (!lrclk) v &= ~(1 << 3);                // audio frame clock -> PB3 (polled by the OS)
        if (audio_button[0]) v &= ~(1 << 5);       // Audio Track A button -> PB5 (function code 8)
        if (audio_button[1]) v &= ~(1 << 6);       // Audio Track B button -> PB6 (function code 9)
        if (patch_button[0]) v &= ~(1 << 10);      // Patch Select left  -> PB10 (falling edge = interrupt)
        if (patch_button[1]) v &= ~(1 << 11);      // Patch Select right -> PB11
        return v;
    };
    duart.irq = [this](bool s) { imp.set_irq_pin(6, s); };
    imp.portb_out = [this](uint16_t dat, uint16_t ddr) {
        pb_out = dat & ddr;
        if (DBG(ASR_DBG_PB)) fprintf(stderr, "PB dat=%04x ddr=%04x t=%.4f\n", dat, ddr, total_cycles / cpu_clock);
    };
    imp.porta_out = [this](uint16_t dat, uint16_t ddr) {
        pa_dat = dat; pa_ddr = ddr;
        bool run = !(ddr & 0x10) || (dat & 0x10);
        esp.set_halt(!run);          // PA4 low asserts ESP HALT
        if (DBG(ASR_DBG_PA)) fprintf(stderr, "PA dat=%04x ddr=%04x t=%.4f\n", dat, ddr, total_cycles / cpu_clock);
    };
    duart.tx = [this](int ch, uint8_t b) {
        if (ch == 0) { if (midi_out) midi_out(b); }
        else {
            if (panel.log) fprintf(stderr, "[PANEL %.4f pc=%06x] %02x %c\n", total_cycles / cpu_clock, last_pc(), b, (b >= 0x20 && b < 0x7f) ? b : '.');
            panel.rx_from_cpu(b);
        }
    };
    panel.send = [this](uint8_t b) { duart.rx_byte(1, b); };
    otto.irq_cb = [this](bool s) {
        otto_irq = s;
        imp.pb_pin(9, !s);
    };
    // OTTO sample fetches go through a per-voice page map written by the OS into CS1
    // ($FF7F00 + 8*slot: four words = CPU 1MB chunk for OTTO address bits 21-20).
    // The slot lags the OTTO voice number by one.
    otto.read_sample = [this](int, uint32_t addr) -> uint16_t {
        int slot = (otto.cur_voice + voice_slot_offset) & 31;
        int page = (addr >> 19) & 3;
        int o = 0x1f00 + slot * 8 + page * 2;
        uint32_t chunk = ((cs1[o] << 8) | cs1[o + 1]) & 0xf;
        uint32_t ba = ((chunk << 20) | ((addr << 1) & 0xfffff)) & ram_mask;
        return (ram[ba] << 8) | ram[(ba + 1) & ram_mask];
    };
    // analog controllers: a 4051-style mux selected by PB2-PB0 feeds the OTTO POT A/D
    otto.read_pot = [this]() -> uint16_t {
        if (DBG(ASR_DBG_POT)) fprintf(stderr, "POT sel=%04x pc=%06x t=%.4f\n", pb_out, last_pc(), total_cycles / cpu_clock);
        return analog[pb_out & 7] & 0x3ff;
    };
    // FDC INT and the WD33C93 INTRQ share IRQ1 (the OS handler checks which device is active)
    fdc.irq_cb = [this](bool) { imp.set_irq_pin(1, fdc.irq() || (scsi_fitted && scsi.irq())); };
    scsi.irq_cb = [this](bool) { imp.set_irq_pin(1, fdc.irq() || (scsi_fitted && scsi.irq())); };
    imp.idma_done = [this]() { if (!last_dma_scsi) fdc.tc(); };
}

void Asr10::set_ram_mb(int mb) {
    uint32_t sz = (uint32_t)mb << 20;
    // round up to power of two for mirroring
    uint32_t p = 1; while (p < sz) p <<= 1;
    ram.assign(p, 0);
    ram_mask = p - 1;
}

bool Asr10::load_rom(const std::string &lo, const std::string &hi) {
    FILE *fl = u8fopen(lo.c_str(), "rb"), *fh = u8fopen(hi.c_str(), "rb");
    if (!fl || !fh) { if (fl) fclose(fl); if (fh) fclose(fh); return false; }
    std::vector<uint8_t> l(0x20000), h(0x20000);
    size_t nl = fread(l.data(), 1, l.size(), fl), nh = fread(h.data(), 1, h.size(), fh);
    fclose(fl); fclose(fh);
    if (nl != nh || nl == 0) return false;
    rom.resize(nl * 2);
    for (size_t i = 0; i < nl; i++) { rom[i * 2] = h[i]; rom[i * 2 + 1] = l[i]; }
    return true;
}

// Try the default names in the given directory, then the original EPROM dump names.
bool Asr10::load_rom_dir(const std::string &dir) {
    static const char *pairs[][2] = {
        { "asr_lo.bin", "asr_hi.bin" },
        { "asr-648c-lo-1.5b.bin", "asr-65e0-hi-1.5b.bin" },
    };
    for (auto &p : pairs)
        if (load_rom(dir + "/" + p[0], dir + "/" + p[1])) return true;
    return load_rom_interleaved(dir + "/asr10_rom.bin");
}

bool Asr10::load_rom_interleaved(const std::string &path) {
    FILE *f = u8fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    rom.resize(sz);
    bool ok = fread(rom.data(), 1, sz, f) == (size_t)sz;
    fclose(f);
    return ok;
}

void Asr10::set_patch_select(int i, bool down) {
    if (i < 0 || i > 1 || patch_button[i] == down) return;
    patch_button[i] = down;
    imp.pb_pin(10 + i, !down);                 // a press is a falling edge: PB10/PB11 interrupt, then the OS polls the pin
}

void Asr10::reset() {
    imp.reset();
    duart.reset();
    scsi.reset();
    fdc.reset();
    otto.reset();
    esp.reset();
    panel.reset();
    rebuild_map();
    m68k_init();
    m68k_set_cpu_type(M68K_CPU_TYPE_68000);
    m68k_pulse_reset();
    total_cycles = 0;
}

uint32_t Asr10::last_pc() const { return m68k_get_reg(NULL, M68K_REG_PPC); }

void Asr10::rebuild_map() {
    for (int p = 0; p < 2048; p++) {
        uint32_t a = (uint32_t)p << 13;
        for (int w = 0; w < 2; w++) {
            PageType t;
            if (imp.is_internal(a)) t = P_INT;
            else {
                int cs = imp.chip_select(a, w != 0);
                switch (cs) {
                case 0: t = P_ROM; break;
                case 1: t = P_CS1; break;   // CS1 (write-only, $FF6000-$FF7FFF): OTTO page map
                case 2: t = P_OTTO; break;
                case 3: t = P_IO3; break;
                default: t = P_RAM; break;
                }
            }
            (w ? wmap : rmap)[p] = t;
        }
    }
}

uint8_t Asr10::cs3_read8(uint32_t a) {
    uint32_t off = a & 0x1fff;
    switch (off >> 11) {
    case 0: return fdc.read((off >> 1) & 1);
    case 1: return duart.read((off >> 1) & 15);
    case 2:   // SCSI (SP-3): $FC5001 address/aux status, $FC5003 data, $FC5401 ID latch
        if (off & 0x400) return 0xff;
        if (!scsi_fitted) return 0xff;
        { uint8_t v = scsi.read((off >> 1) & 1); if (trace_io) fprintf(stderr, "SCSI R %d = %02x pc=%06x\n", (off >> 1) & 1, v, last_pc()); return v; }
    case 3:   // DMA data port, shared by the floppy controller and the SCSI chip
        if (scsi.dma_active()) { last_dma_scsi = true; return scsi.dma_read(); }
        last_dma_scsi = false;
        return fdc.dma_read();
    }
    return 0xff;
}

void Asr10::cs3_write8(uint32_t a, uint8_t v) {
    uint32_t off = a & 0x1fff;
    switch (off >> 11) {
    case 0: fdc.write((off >> 1) & 1, v); break;
    case 1:
        if (trace_io && ((off >> 1) & 15) == 11) fprintf(stderr, "PANELTX %02x pc=%06x t=%.4f\n", v, last_pc(), cycles() / cpu_clock);
        duart.write((off >> 1) & 15, v); break;
    case 2:
        if (off & 0x400) { scsi_latch = v; break; }
        if (trace_io) fprintf(stderr, "SCSI W %d = %02x pc=%06x\n", (off >> 1) & 1, v, last_pc());
        if (scsi_fitted) scsi.write((off >> 1) & 1, v);
        break;
    case 3:
        if (scsi.dma_active()) { last_dma_scsi = true; scsi.dma_write(v); break; }
        last_dma_scsi = false;
        fdc.dma_write(v); break;
    }
}

uint32_t Asr10::io_read(PageType t, uint32_t a, int sz) {
    if (sz == 4) return (io_read(t, a, 2) << 16) | io_read(t, a + 2, 2);
    switch (t) {
    case P_INT:
        return sz == 1 ? imp.read8(a) : imp.read16(a);
    case P_OTTO: {
        // CS2: FC2000 OTTO, FC3000 ESP; 8-bit devices on D7-D0 (odd addresses)
        if (a & 0x1000) {
            uint8_t v = esp.host_r((a >> 1) & 0xff);
            if (trace_io) fprintf(stderr, "ESP R %06x (%02x) = %02x pc=%06x\n", a, (a >> 1) & 0xff, v, last_pc());
            return sz == 1 ? ((a & 1) ? v : 0xff) : (0xff00 | v);
        }
        uint8_t v = otto.read((a >> 1) & 0x3f);
        if (trace_io) fprintf(stderr, "OTTO R %06x = %02x pc=%06x\n", a, v, last_pc());
        return sz == 1 ? ((a & 1) ? v : 0xff) : (0xff00 | v);
    }
    case P_IO3: {
        uint8_t v = cs3_read8(a | 1);
        if (trace_io) fprintf(stderr, "IO3 R %06x = %02x pc=%06x\n", a, v, last_pc());
        return sz == 1 ? ((a & 1) ? v : 0xff) : (0xff00 | v);
    }
    default:
        if (trace_unknown && unknown_log_budget > 0) { unknown_log_budget--; fprintf(stderr, "?? R%d %06x pc=%06x\n", sz * 8, a, last_pc()); }
        return sz == 1 ? 0xff : 0xffff;
    }
}

void Asr10::io_write(PageType t, uint32_t a, uint32_t v, int sz) {
    if (sz == 4) { io_write(t, a, v >> 16, 2); io_write(t, a + 2, v & 0xffff, 2); return; }
    switch (t) {
    case P_INT: {
        uint32_t off = a & 0xfff;
        if (sz == 1) imp.write8(a, v); else imp.write16(a, v);
        if (off >= 0x830 && off < 0x840) rebuild_map();
        return;
    }
    case P_OTTO:
        if (a & 0x1000) {
            if (sz == 2 || (a & 1)) esp.host_w((a >> 1) & 0xff, v & 0xff);
            if (trace_io) fprintf(stderr, "ESP W %06x (%02x) = %02x pc=%06x\n", a, (a >> 1) & 0xff, v & 0xff, last_pc());
            return;
        }
        if (sz == 2 || (a & 1)) otto.write((a >> 1) & 0x3f, v & 0xff);
        if (trace_io) fprintf(stderr, "OTTO W %06x = %02x pc=%06x\n", a, v & 0xff, last_pc());
        return;
    case P_IO3:
        if (sz == 2 || (a & 1)) cs3_write8(a | 1, v & 0xff);
        if (trace_io) fprintf(stderr, "IO3 W %06x = %02x pc=%06x\n", a, v & 0xff, last_pc());
        return;
    case P_ROM:
        return;
    case P_CS1: {
        uint32_t o = a & 0x1fff;
        for (int i = 0; i < sz; i++) cs1[(o + i) & 0x1fff] = v >> (8 * (sz - 1 - i));
        {   // the same write also lands in RAM (the OS reads its page map back from there)
            uint32_t r = a & ram_mask;
            for (int i = 0; i < sz; i++) ram[(r + i) & ram_mask] = v >> (8 * (sz - 1 - i));
        }
        if (DBG(ASR_DBG_CS1)) fprintf(stderr, "CS1 W%d %06x = %x pc=%06x t=%.4f\n", sz * 8, a, v, last_pc(), total_cycles / cpu_clock);
        return;
    }
    default:
        if (trace_unknown && unknown_log_budget > 0) { unknown_log_budget--; fprintf(stderr, "?? W%d %06x = %x pc=%06x\n", sz * 8, a, v, last_pc()); }
        return;
    }
}

uint32_t Asr10::read(uint32_t a, int sz) {
    a &= 0xffffff;
    if (a < 0xf8 && a + sz > 0xf0) { uint32_t v = 0; for (int i = 0; i < sz; i++) v = (v << 8) | ((a + i >= 0xf0 && a + i < 0xf8) ? imp.sysreg_read8(a + i) : read(a + i, 1)); return v; }
    PageType t = rmap[a >> 13];
    if (t == P_RAM) {
        uint32_t o = a & ram_mask;
        if (sz == 1) return ram[o];
        if (sz == 2) return (ram[o] << 8) | ram[(o + 1) & ram_mask];
        return ((uint32_t)ram[o] << 24) | (ram[(o + 1) & ram_mask] << 16) | (ram[(o + 2) & ram_mask] << 8) | ram[(o + 3) & ram_mask];
    }
    if (t == P_ROM) {
        uint32_t m = (uint32_t)rom.size() - 1, o = a & m;
        if (sz == 1) return rom[o];
        if (sz == 2) {
            uint16_t w = (rom[o] << 8) | rom[(o + 1) & m];
            // The Load page lights an indicator for the type of the file shown, from a table in the
            // ROM ($F82546, one word per file type). It ends at type $21, so for an ASR-10 macro
            // file (type $22) the OS reads past it and lights nothing, while EPS macros (type 9)
            // light MACRO. Give type $22 the macro entry, so that ASR macros light MACRO too.
            if (a == 0xf8258a && w == 0x0004 && read(0xf82558, 2) == 0x258d) {
                uint32_t pc = last_pc() & 0xffffff;
                if (read(pc, 2) == 0x3474 && read(pc + 2, 2) == 0x2000) w = 0x258d;   // movea.w (A4,D2.w),A2
            }
            return w;
        }
        return ((uint32_t)rom[o] << 24) | (rom[(o + 1) & m] << 16) | (rom[(o + 2) & m] << 8) | rom[(o + 3) & m];
    }
    return io_read(t, a, sz);
}

void Asr10::write(uint32_t a, uint32_t v, int sz) {
    a &= 0xffffff;
    if (a < 0xf8 && a + sz > 0xf0) {
        for (int i = 0; i < sz; i++) {
            uint8_t b = v >> (8 * (sz - 1 - i));
            if (a + i >= 0xf0 && a + i < 0xf8) imp.sysreg_write8(a + i, b); else write(a + i, b, 1);
        }
        rebuild_map();
        return;
    }
    PageType t = wmap[a >> 13];
    if (t == P_RAM) {
        uint32_t o = a & ram_mask;
        if (sz == 1) ram[o] = v;
        else if (sz == 2) { ram[o] = v >> 8; ram[(o + 1) & ram_mask] = v; }
        else { ram[o] = v >> 24; ram[(o + 1) & ram_mask] = v >> 16; ram[(o + 2) & ram_mask] = v >> 8; ram[(o + 3) & ram_mask] = v; }
        return;
    }
    io_write(t, a, v, sz);
}

// --------------------------------------------------------------- scheduling
static inline int16_t to16(int32_t v20) {
    int32_t v = v20 >> 4;
    if (v > 32767) v = 32767; if (v < -32768) v = -32768;
    return (int16_t)v;
}

void Asr10::feed_input(const float *lr, size_t frames, double rate) {
    std::lock_guard<std::mutex> g(input_mx);
    input_rate = rate;
    for (size_t i = 0; i < frames * 2; i++) input.push_back(lr[i]);
    // keep at most ~1 s queued (drop the oldest if the producer runs ahead)
    size_t maxs = (size_t)(rate * 0.25) * 2;   // keep latency below ~250 ms
    while (input.size() > maxs) { input.pop_front(); input.pop_front(); }
    in_count = input.size();
}

// A/D input: one stereo sample per audio frame, shifted MSB first into SCC1 (left)
// and SCC2 (right), which DMA it into the sampling buffers in RAM.
void Asr10::input_frame(double frame_rate) {
    float l = in_last_l, r = in_last_r;      // on underrun hold the last value
    if (!imp.scc_sync_active(0) && !imp.scc_sync_active(1) && in_count.load() == 0) return;
    {
        std::lock_guard<std::mutex> g(input_mx);
        if (input.size() >= 4) {
            float t = (float)in_pos;
            l = input[0] + (input[2] - input[0]) * t;
            r = input[1] + (input[3] - input[1]) * t;
            in_pos += input_rate / frame_rate;
            while (in_pos >= 1.0 && input.size() >= 4) { in_pos -= 1.0; input.pop_front(); input.pop_front(); }
            in_count = input.size();
        }
    }
    in_last_l = l; in_last_r = r;
    { const float g = input_gain.load(std::memory_order_relaxed); l *= g; r *= g; }   // Input Level pot
    l = std::clamp(l, -1.0f, 1.0f); r = std::clamp(r, -1.0f, 1.0f);                  // the A/D clips
    in_peak = std::max(in_peak, std::max(std::fabs(l), std::fabs(r)));
    in_peak_ch[0] = std::max(in_peak_ch[0], std::fabs(l)); in_peak_ch[1] = std::max(in_peak_ch[1], std::fabs(r));
    if (DBG(ASR_DBG_IN)) { static uint64_t n = 0; if ((++n % 29760) == 0) fprintf(stderr, "IN frames=%llu t=%.3f fr=%.1f q=%zu voices=%d\n", (unsigned long long)n, total_cycles / cpu_clock, frame_rate, input.size(), otto.active_voices()); }
    auto to_s16 = [](float x) -> int16_t { int v = (int)lrintf(x * 32767.0f); return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); };
    int16_t sl = to_s16(l), sr = to_s16(r);
    if (imp.scc_sync_active(0)) { imp.scc_sync_rx(0, (uint8_t)(sl >> 8)); imp.scc_sync_rx(0, (uint8_t)sl); }
    if (imp.scc_sync_active(1)) { imp.scc_sync_rx(1, (uint8_t)(sr >> 8)); imp.scc_sync_rx(1, (uint8_t)sr); }
}

void Asr10::otto_frame() {
    int32_t ch[12];
    otto.generate_frame(ch);
    lrclk = !lrclk;
    input_frame(otto.sample_rate(otto_master()));
    double l = 0, r = 0;
    if (fx_enabled) {
        // OTTO serial channels feed the ESP serial inputs; ESP SER1 drives the main DAC
        static const char *tsine = getenv("ASR_TEST_SINE");   // debug: freq[:amp] sine into ESP input 1
        if (tsine) {
            static double ph = 0; double f = atof(tsine), am = strchr(tsine, ':') ? atof(strchr(tsine, ':') + 1) : 0.25;
            int32_t v = (int32_t)(sin(ph) * am * 524287.0);
            ph += 2 * M_PI * f / otto.sample_rate(otto_master());
            for (int p = 0; p < 4; p++) if (esp_route[p] >= 0) { ch[esp_route[p] * 2] = v; ch[esp_route[p] * 2 + 1] = v; break; }
        }
        for (int p = 0; p < 4; p++) {
            int c = esp_route[p];
            if (c < 0) continue;
            esp.ser_w(p * 2, to16(ch[c * 2]));
            esp.ser_w(p * 2 + 1, to16(ch[c * 2 + 1]));
        }
        esp.run_frame();
        l = esp.ser_r(2) * 16.0; r = esp.ser_r(3) * 16.0;
        if (esp.halted()) {   // while the OS reloads the ESP the outputs are silent on the real unit
            l = r = 0;
        }
        for (int c = 0; c < 6; c++)
            if ((direct_mask & (1 << c)) && (c < 3 || aux_to_main[c - 3])) { l += ch[c * 2]; r += ch[c * 2 + 1]; }
    } else {
        for (int c = 0; c < 6; c++)
            if (c < 3 || aux_to_main[c - 3]) { l += ch[c * 2]; r += ch[c * 2 + 1]; }
    }
    for (int c = 0; c < 6; c++) { chan_peak[c] = std::max(chan_peak[c], std::abs(ch[c * 2]) + 0); }
    const float k = 1.0f / 524288.0f;
    float cur[8] = { (float)l * k, (float)r * k };
    const bool sep = aux_separate();
    for (int a = 0; a < 3; a++) if (sep && !aux_to_main[a]) { cur[2 + a * 2] = ch[(3 + a) * 2] * k; cur[3 + a * 2] = ch[(3 + a) * 2 + 1] * k; }
    // simple linear resampler to out_rate
    double in_rate = otto.sample_rate(otto_master());
    double step = in_rate / out_rate;
    while (resamp_pos < 1.0) {
        float t = (float)resamp_pos;
        audio.push_back(last[0] + (cur[0] - last[0]) * t);
        audio.push_back(last[1] + (cur[1] - last[1]) * t);
        if (sep) for (int i = 2; i < 8; i++) audio_aux.push_back(last[i] + (cur[i] - last[i]) * t);
        resamp_pos += step;
    }
    resamp_pos -= 1.0;
    for (int i = 0; i < 8; i++) last[i] = cur[i];
}

bool Asr10::insert_disk(const std::string &path) {
    if (fdc.has_disk() && fdc.dirty) fdc.save();
    bool ok = fdc.load(path);
    disk_change_pulse();
    return ok;
}

// The drive's disk-change line is wired to DUART IP1; the OS looks at the IPCR
// delta bit (bit 5) after every SENSE DRIVE STATUS to notice a swapped disk.
void Asr10::disk_change_pulse() {
    dskchg_level = !dskchg_level;
    duart.set_ip(1, dskchg_level);
}

void Asr10::eject_disk() {
    if (fdc.has_disk() && fdc.dirty) fdc.save();
    fdc.eject();
    disk_change_pulse();
}

void Asr10::run(int64_t clocks) {
    int64_t target = total_cycles + clocks;
    while ((int64_t)total_cycles < target) {
        int c = m68k_execute(32);
        total_cycles += c;
        imp.run(c);
        fdc.run(c);
        duart_acc += c * (duart.xtal / cpu_clock);
        if (duart_acc >= 16) { duart.run(duart_acc); duart_acc = 0; }
        otto_acc += c * (otto_master() / cpu_clock);
        double frame_clocks = 16.0 * (otto.active_voices() + 1);
        while (otto_acc >= frame_clocks) { otto_acc -= frame_clocks; otto_frame(); }
        // drive READY -> DUART IP0 (change detected by the OS), HD sensor -> IP2
        if (fdc.motor & 1) { if (motor_time < 4000000) motor_time += c; } else motor_time = 0;
        bool ready = motor_time >= 3200000 && fdc.has_disk();   // ~200 ms spin-up
        if (ready != drive_ready) { drive_ready = ready; duart.set_ip(0, !ready); }
        duart.set_ip(2, fdc.is_hd());
    }
}
