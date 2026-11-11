// Ensoniq ASR-10 machine: MC68302 + ES5506 OTTO + ES5510 ESP + uPD72069 FDC + 2681 DUART
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <mutex>
#include <atomic>
#include "m68302.h"
#include "duart.h"
#include "fdc.h"
#include "es5506.h"
#include "es5510.h"
#include "panel.h"
#include "scsi.h"

class Asr10 {
public:
    Asr10();
    bool load_rom(const std::string &lo, const std::string &hi);   // split 27C010 pair
    bool load_rom_interleaved(const std::string &path);
    bool load_rom_dir(const std::string &dir);                       // asr_lo/hi.bin or the original dump names
    void set_ram_mb(int mb);
    void reset();

    // run for the given number of CPU clocks
    void run(int64_t clocks);
    uint64_t cycles() const { return total_cycles; }

    // floppy drive
    bool insert_disk(const std::string &path);
    void eject_disk();
    void disk_change_pulse();
    bool dskchg_level = true;

    // audio input (A/D): interleaved stereo float at `rate`; thread safe
    void feed_input(const float *lr, size_t frames, double rate);
    float in_peak = 0;                  // input peak meter (reset by the consumer)
    std::atomic<float> input_gain{1.0f}; // Input Level pot: gain before the A/D (clips at full scale)
    float in_peak_ch[2] = { 0, 0 };
    // Audio Track A/B buttons: wired straight to PB5/PB6 of the 68302 (active low), not through the
    // panel micro. Their LEDs are 68302 port A outputs (active low): PA5/PA6 = A/B "Selected"
    // (yellow), PA11/PA12 = A/B "Source Monitor" (red).
    bool audio_button[2] = { false, false };
    uint16_t pa_dat = 0xffff, pa_ddr = 0;
    bool audio_led(int n) const {            // 0/1 = Selected A/B, 2/3 = Source Monitor A/B
        static const int bit[4] = { 5, 6, 11, 12 };
        return (pa_ddr >> bit[n] & 1) && !(pa_dat >> bit[n] & 1);
    }     // left / right input peaks for the front panel LEDs (reset by the consumer)

    // MIDI in/out
    void midi_in(uint8_t b) { duart.rx_byte(0, b); }
    std::function<void(uint8_t)> midi_out;

    // audio: stereo float frames at native OTTO rate are resampled to out_rate
    double out_rate = 44100.0;
    std::vector<float> audio;          // interleaved L/R, consumer drains it
    // Output Expander (AUX1-3): when aux_to_main[i] is false, AUX i+1 is left out of the main mix
    // and delivered on its own in audio_aux (6 interleaved channels, same frames as `audio`)
    bool aux_to_main[3] = { true, true, true };
    bool aux_separate() const { return !aux_to_main[0] || !aux_to_main[1] || !aux_to_main[2]; }
    std::vector<float> audio_aux;
    bool fx_enabled = true;

    // memory access (bus)
    uint32_t read(uint32_t a, int sz);
    void     write(uint32_t a, uint32_t v, int sz);

    M68302 imp;
    Duart  duart;
    Fdc    fdc;
    ES5506 otto;
    ES5510 esp;
    Panel  panel;
    Wd33c93 scsi;
    bool scsi_fitted = true;           // SP-3 SCSI option present
    uint8_t scsi_latch = 0;
    bool last_dma_scsi = false;

    std::vector<uint8_t> rom;
    std::vector<uint8_t> ram;
    uint32_t ram_mask = 0;
    uint8_t cs1[0x2000] = {};
    int voice_slot_offset = 31;          // CS1 write-only registers (OTTO page map)

    double cpu_clock = 16000000.0;
    double otto_clock = 30476180.0 / 2;   // 15.238 MHz -> 29.76 kHz with 32 voices
    double otto_clock_44k = 33868800.0 / 2;   // 16.934 MHz -> 44.1 kHz with 24 voices
    // The 44.1 kHz effects run the OTTO with 24 voice slots from a second clock (the OS shows
    // "23 VOICE 44K"); the select line is not identified, so the clock follows the slot count.
    double otto_master() const { return otto.active_voices() + 1 <= 24 ? otto_clock_44k : otto_clock; }
    int esp_route[4] = { 0, -1, 1, 2 };   // OTTO channel feeding ESP SER0..SER3 (-1 = none)
    int direct_mask = 0x38;               // OTTO channels wired straight to the outputs (aux)
    int32_t chan_peak[6] = {};

    bool trace_io = false;
    bool trace_unknown = true;
    uint32_t last_pc() const;

    // diagnostic hooks
    std::function<void(uint32_t)> on_instr;

private:
    enum PageType : uint8_t { P_RAM, P_ROM, P_INT, P_OTTO, P_IO3, P_CS1, P_NONE };
    PageType rmap[2048], wmap[2048];
    void rebuild_map();
    uint32_t io_read(PageType t, uint32_t a, int sz);
    void io_write(PageType t, uint32_t a, uint32_t v, int sz);
    uint8_t cs3_read8(uint32_t a);
    void cs3_write8(uint32_t a, uint8_t v);

    uint64_t total_cycles = 0;
    double otto_acc = 0, duart_acc = 0;
    double resamp_pos = 0;
    float last[8] = {};
    void otto_frame();
    void input_frame(double frame_rate);
    std::mutex input_mx;
    std::deque<float> input;
    std::atomic<size_t> in_count{0};
    double input_rate = 44100, in_pos = 0;
    float in_last_l = 0, in_last_r = 0;
    bool otto_irq = false;
    bool drive_ready = false;
    bool lrclk = false;
    uint16_t pb_out = 0;
public:
    // 10-bit A/D values by mux channel (PB2-0; the OS scans 0,2,3,4,5,7 with PB4 set):
    //   0 pitch wheel (centre calibrated at power-up)  2 mod wheel  3 volume slider
    //   4 CV pedal (unplugged = full)  5 data entry slider  7 reference voltage
    enum { AN_PITCH = 0, AN_MOD = 2, AN_VOLUME = 3, AN_PEDAL = 4, AN_DATA = 5, AN_REF = 7 };
    uint16_t analog[8] = { 512, 0, 0, 1000, 1023, 512, 0, 800 };
private:
    int64_t motor_time = 0;
    int  unknown_log_budget = 400;
};

extern Asr10 *g_machine;
