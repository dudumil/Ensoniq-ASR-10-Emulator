// Headless runner for the ASR-10 emulator: boots, optionally plays MIDI events,
// renders audio to a WAV file and prints the display.
#include "asr10.h"
#include "efe_disk.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
extern "C" {
#include "m68k.h"
}

static void write_wav(const char *path, const std::vector<float> &a, int rate) {
    FILE *f = fopen(path, "wb"); if (!f) return;
    uint32_t n = (uint32_t)a.size();
    uint32_t datasz = n * 2;
    auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); w32(36 + datasz); fwrite("WAVEfmt ", 1, 8, f);
    w32(16); w16(1); w16(2); w32(rate); w32(rate * 4); w16(4); w16(16);
    fwrite("data", 1, 4, f); w32(datasz);
    for (float x : a) { int v = (int)(x * 32767.0f); if (v > 32767) v = 32767; if (v < -32768) v = -32768; w16((uint16_t)(int16_t)v); }
    fclose(f);
}

int main(int argc, char **argv) {
    std::string rom_dir = "roms"; bool lo_set = false, hi_set = false;
    std::string lo = "roms/asr_lo.bin", hi = "roms/asr_hi.bin", disk, wav, aux_wav;
    bool m_aux_flag[3] = { true, true, true };   // AUX1-3 mixed into the main outputs
    double seconds = 10;
    bool write_back = false;
    std::vector<std::string> scsi_dirs, scsi_specs; bool scsi_log = false, no_scsi = false;
    std::vector<std::pair<int,int>> analog_init;
    std::vector<std::pair<double,std::pair<int,int>>> patchev;
    std::vector<std::pair<double, std::pair<int,int>>> analog_at;
    bool trace_io = false, panel_log = false, fdc_log = false, trap_log = false, pc_hist = false;
    std::vector<std::pair<double, std::vector<uint8_t>>> midi;   // time, bytes
    std::vector<std::pair<double, std::pair<int,int>>> buttons;  // time, (code, down)
    int ram_mb = 16;   // maximum memory expansion (2 MB = base unit)
    std::vector<std::pair<uint32_t, int>> trace_pcs;
    std::vector<uint32_t> breaks;
    std::vector<std::pair<double, std::pair<int,int>>> keys;
    std::vector<std::pair<double, std::string>> inserts;
    std::vector<std::pair<double, std::pair<int,int>>> raws;
    double trace_after = 0;
    uint32_t trace_pc = 0xffffffff; int trace_cnt = 0; bool tracing = false;
    std::string dump_ram; bool rack_panel = false;
    bool esp_list = false, otto_dump = false;
    uint32_t dis_addr = 0; int dis_n = 0;
    uint32_t hex_addr = 0; int hex_len = 0;
    struct Tone { double t, f, amp, dur; char side; };
    std::vector<Tone> tones;
    std::string in_wav; double in_wav_t = 0;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        auto next = [&]() { return std::string(i + 1 < argc ? argv[++i] : ""); };
        if (a == "--lo") { lo = next(); lo_set = true; }
        else if (a == "--hi") { hi = next(); hi_set = true; }
        else if (a == "--roms") rom_dir = next();
        else if (a == "--disk") disk = next();
        else if (a == "--seconds") seconds = atof(next().c_str());
        else if (a == "--aux-wav") { aux_wav = next(); for (auto &x : m_aux_flag) x = false; }
        else if (a == "--wav") wav = next();
        else if (a == "--ram") ram_mb = atoi(next().c_str());
        else if (a == "--trace-io") trace_io = true;
        else if (a == "--write-back") write_back = true;
        else if (a == "--scsi-dir") scsi_dirs.push_back(next());
        else if (a == "--scsi") scsi_specs.push_back(next());     // id:path[:cd]
        else if (a == "--scsi-log") scsi_log = true;
        else if (a == "--no-scsi") no_scsi = true;
        else if (a == "--analog-at") { double t; int ch, v; if (sscanf(next().c_str(), "%lf:%d:%d", &t, &ch, &v) == 3 && ch >= 0 && ch < 8) analog_at.push_back({t, {ch, v}}); }
        else if (a == "--analog") { int ch, v; if (sscanf(next().c_str(), "%d:%d", &ch, &v) == 2 && ch >= 0 && ch < 8) analog_init.push_back({ch, v}); }
        else if (a == "--panel-log") panel_log = true;
        else if (a == "--fdc-log") fdc_log = true;
        else if (a == "--trap-log") trap_log = true;
        else if (a == "--dump-ram") dump_ram = next();
        else if (a == "--esp-list") esp_list = true;
        else if (a == "--rack") rack_panel = true;
        else if (a == "--otto-dump") otto_dump = true;
        else if (a == "--dis") { dis_addr = strtoul(next().c_str(), 0, 16); dis_n = atoi(next().c_str()); }
        else if (a == "--pc-hist") pc_hist = true;
        else if (a == "--input-tone") {  // t:freq:amp:dur[:L|R]  sine into the audio input
            std::string v = next(); double t, f, am, d; char side = 'B';
            if (sscanf(v.c_str(), "%lf:%lf:%lf:%lf:%c", &t, &f, &am, &d, &side) >= 4) tones.push_back({t, f, am, d, side});
        }
        else if (a == "--input-wav") {   // t:file.wav (16-bit PCM)
            std::string v = next(); size_t c = v.find(':');
            if (c != std::string::npos) { in_wav_t = atof(v.substr(0, c).c_str()); in_wav = v.substr(c + 1); }
        }
        else if (a == "--hexdump") { hex_addr = strtoul(next().c_str(), 0, 16); hex_len = atoi(next().c_str()); }
        else if (a == "--break") breaks.push_back(strtoul(next().c_str(), 0, 16));
        else if (a == "--trace-after") trace_after = atof(next().c_str());
        else if (a == "--trace-from") { trace_pc = strtoul(next().c_str(), 0, 16); trace_cnt = atoi(next().c_str()); }
        else if (a == "--note") {   // --note time:note:vel:dur
            double t; int n, v, ch = 1; double d;
            if (sscanf(next().c_str(), "%lf:%d:%d:%lf:%d", &t, &n, &v, &d, &ch) >= 4) {   // [:channel]
                midi.push_back({t, {(uint8_t)(0x90 | ((ch - 1) & 15)), (uint8_t)n, (uint8_t)v}});
                midi.push_back({t + d, {(uint8_t)(0x80 | ((ch - 1) & 15)), (uint8_t)n, 0x40}});
            }
        } else if (a == "--patch") {   // --patch time:0|1:down  Patch Select left/right
            double t; int i, d; if (sscanf(next().c_str(), "%lf:%d:%d", &t, &i, &d) == 3) patchev.push_back({t, {i, d}});
        } else if (a == "--cc") {      // --cc time:controller:value[:channel]  MIDI control change
            double t; int c, v, ch = 1;
            if (sscanf(next().c_str(), "%lf:%d:%d:%d", &t, &c, &v, &ch) >= 3) midi.push_back({t, {(uint8_t)(0xb0 | ((ch - 1) & 15)), (uint8_t)c, (uint8_t)v}});
        } else if (a == "--key") {   // --key time:note:vel:dur (panel keyboard, note 36..96)
            double t; int n, v; double d;
            if (sscanf(next().c_str(), "%lf:%d:%d:%lf", &t, &n, &v, &d) == 4) {
                keys.push_back({t, {n, v}}); keys.push_back({t + d, {n, -v}});
            }
        } else if (a == "--insert") {  // time:path  (disk inserted while running)
            std::string v = next(); size_t c = v.find(':');
            if (c != std::string::npos) inserts.push_back({atof(v.substr(0, c).c_str()), v.substr(c + 1)});
        } else if (a == "--panel-raw") {  // time:b1:b2 (hex) raw panel->CPU message
            double t; unsigned b1, b2;
            if (sscanf(next().c_str(), "%lf:%x:%x", &t, &b1, &b2) == 3) raws.push_back({t, {(int)b1, (int)b2}});
        } else if (a == "--eject") {
            inserts.push_back({atof(next().c_str()), ""});
        } else if (a == "--button") {  // time:code:down
            double t; int c, d;
            if (sscanf(next().c_str(), "%lf:%d:%d", &t, &c, &d) == 3) buttons.push_back({t, {c, d}});
        }
    }
    Asr10 m;
    m.set_ram_mb(ram_mb);
    for (int i = 0; i < 3; i++) m.aux_to_main[i] = m_aux_flag[i];
    bool rom_ok = (lo_set || hi_set) ? m.load_rom(lo, hi) : (m.load_rom_dir(rom_dir) || m.load_rom(lo, hi));
    if (!rom_ok) { fprintf(stderr, "cannot load ROM %s / %s\n", lo.c_str(), hi.c_str()); return 1; }
    if (!disk.empty() && !m.fdc.load(disk)) { fprintf(stderr, "cannot load disk %s\n", disk.c_str()); return 1; }
    m.trace_io = trace_io;
    m.panel.log = panel_log;
    m.panel.rack = rack_panel;
    m.fdc.log = fdc_log;
    m.reset();
    m.scsi_fitted = !no_scsi;
    m.scsi.log = scsi_log;
    for (auto &d : scsi_dirs) fprintf(stderr, "%s", m.scsi.attach_dir(d).c_str());
    for (auto &sp : scsi_specs) {
        int id = atoi(sp.c_str()); size_t c = sp.find(':');
        std::string path = c == std::string::npos ? "" : sp.substr(c + 1);
        bool cd = path.size() > 3 && path.substr(path.size() - 3) == ":cd";
        if (cd) path.resize(path.size() - 3);
        if (!m.scsi.attach(id, path, cd ? ScsiDevice::CD : ScsiDevice::HD)) fprintf(stderr, "cannot attach SCSI %s\n", sp.c_str());
        else fprintf(stderr, "SCSI ID %d: %s\n", id, m.scsi.dev[id]->describe().c_str());
    }
    for (auto &a : analog_init) m.analog[a.first] = a.second;
    // optional WAV for the audio input
    std::vector<float> wav_in; double wav_in_rate = 44100; size_t wav_in_pos = 0;
    if (!in_wav.empty()) {
        FILE *f = fopen(in_wav.c_str(), "rb");
        if (f) {
            std::vector<uint8_t> d; uint8_t buf[65536]; size_t n;
            while ((n = fread(buf, 1, sizeof buf, f)) > 0) d.insert(d.end(), buf, buf + n);
            fclose(f);
            size_t p = 12; int chn = 2, bits = 16;
            while (p + 8 <= d.size()) {
                uint32_t sz = d[p+4] | d[p+5] << 8 | d[p+6] << 16 | (uint32_t)d[p+7] << 24;
                if (!memcmp(&d[p], "fmt ", 4)) { chn = d[p+10] | d[p+11] << 8; wav_in_rate = d[p+12] | d[p+13] << 8 | d[p+14] << 16; bits = d[p+22]; }
                if (!memcmp(&d[p], "data", 4) && bits == 16) {
                    for (size_t i = p + 8; i + 2 * chn <= p + 8 + sz && i + 2 * chn <= d.size(); i += 2 * chn) {
                        int16_t l = (int16_t)(d[i] | d[i+1] << 8), r = chn > 1 ? (int16_t)(d[i+2] | d[i+3] << 8) : l;
                        wav_in.push_back(l / 32768.0f); wav_in.push_back(r / 32768.0f);
                    }
                }
                p += 8 + sz + (sz & 1);
            }
            fprintf(stderr, "input wav: %zu frames at %.0f Hz\n", wav_in.size() / 2, wav_in_rate);
        } else fprintf(stderr, "cannot open %s\n", in_wav.c_str());
    }
    std::map<uint32_t, uint64_t> hist;
    static uint32_t ring[64]; static int rp = 0;
    if (trap_log || pc_hist || !breaks.empty() || trace_cnt) m.on_instr = [&](uint32_t pc) {
        pc &= 0xffffff;
        if ((pc == trace_pc || trace_pc == 1) && m.cycles() / m.cpu_clock >= trace_after) tracing = true;
        if (tracing && trace_cnt > 0) {
            char buf[128]; m68k_disassemble(buf, pc, M68K_CPU_TYPE_68000);
            fprintf(stderr, "T %06x: %-40s D0=%08x D1=%08x D2=%08x A0=%08x\n", pc, buf, m68k_get_reg(NULL, M68K_REG_D0), m68k_get_reg(NULL, M68K_REG_D1), m68k_get_reg(NULL, M68K_REG_D2), m68k_get_reg(NULL, M68K_REG_A0));
            if (--trace_cnt == 0) tracing = false;
        }
        ring[rp++ & 63] = pc;
        for (auto b : breaks) if (pc == b && m.cycles() / m.cpu_clock >= trace_after) {
            fprintf(stderr, "[BREAK] %06x t=%.4f SR=%04x SP=%08x USP=%08x\n", pc, m.cycles() / m.cpu_clock, m68k_get_reg(NULL, M68K_REG_SR), m68k_get_reg(NULL, M68K_REG_SP), m68k_get_reg(NULL, M68K_REG_USP));
            for (int r = 0; r < 8; r++) fprintf(stderr, "  D%d=%08x A%d=%08x\n", r, m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_D0 + r)), r, m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_A0 + r)));
            uint32_t sp = m68k_get_reg(NULL, M68K_REG_SP);
            fprintf(stderr, "  stack:"); for (int k = 0; k < 16; k++) fprintf(stderr, " %04x", m.read(sp + k * 2, 2)); fprintf(stderr, "\n");
            uint32_t usp = m68k_get_reg(NULL, M68K_REG_USP);
            fprintf(stderr, "  ustack:"); for (int k = 0; k < 24; k++) fprintf(stderr, " %04x", m.read(usp + k * 2, 2)); fprintf(stderr, "\n");
            fprintf(stderr, "  recent pcs:"); for (int k = 0; k < 64; k++) fprintf(stderr, " %06x", ring[(rp + k) & 63]); fprintf(stderr, "\n");
        }
        if (pc_hist && m.cycles() / m.cpu_clock >= trace_after) hist[pc & ~0xf]++;
        if (trap_log) {
            uint16_t op = m.read(pc, 2);
            if ((op & 0xfff0) == 0x4e40 && (op & 15) == 0)
                fprintf(stderr, "[TRAP0] pc=%06x D0=%08x t=%.3f\n", pc, m68k_get_reg(NULL, M68K_REG_D0), m.cycles() / m.cpu_clock);
        }
    };
    m.midi_out = [](uint8_t b) { fprintf(stderr, "[MIDI OUT] %02x\n", b); };
    std::sort(midi.begin(), midi.end(), [](auto &x, auto &y) { return x.first < y.first; });
    size_t mi = 0, bi = 0, ki = 0;
    std::sort(keys.begin(), keys.end(), [](auto &x, auto &y) { return x.first < y.first; });
    const int64_t slice = 16000;  // 1 ms
    int64_t total = (int64_t)(seconds * m.cpu_clock);
    std::string last0, last1;
    for (int64_t t = 0; t < total; t += slice) {
        double now = t / m.cpu_clock;
        while (mi < midi.size() && midi[mi].first <= now) { for (auto b : midi[mi].second) m.midi_in(b); mi++; }
        while (ki < keys.size() && keys[ki].first <= now) { auto &k = keys[ki].second; m.panel.key(k.first - 36, k.second < 0 ? -k.second : k.second, k.second > 0); ki++; }
        while (bi < buttons.size() && buttons[bi].first <= now) {
            int idx = buttons[bi].second.first;
            if (idx == 40 || idx == 41) m.audio_button[idx - 40] = buttons[bi].second.second;   // Audio Track A/B
            else m.panel.button(idx, buttons[bi].second.second);
            bi++;
        }
        for (auto &ins : inserts) if (ins.first >= 0 && ins.first <= now) {
            if (!ins.second.empty() && efe::is_single_file(ins.second)) {   // .efa/.efe: on a temporary floppy
                std::string path, label, err = efe::make_disk_file({ ins.second }, true, "asr10-cli-", path, label);
                if (!err.empty()) fprintf(stderr, "%s\n", err.c_str()); else ins.second = path;
            }
            if (ins.second.empty()) m.eject_disk(); else m.insert_disk(ins.second);
            ins.first = -1;
        }
        {   // audio input for this slice
            double sl = slice / m.cpu_clock;
            static double in_acc = 0;
            in_acc += 44100 * sl;
            int nfr = (int)in_acc; in_acc -= nfr;
            static std::vector<float> buf; buf.assign(nfr * 2, 0.0f);
            bool any = false;
            for (auto &tn : tones) if (now >= tn.t && now < tn.t + tn.dur) {
                any = true;
                for (int i = 0; i < nfr; i++) {
                    double tt = now - tn.t + i / 44100.0;
                    float v = (float)(tn.amp * sin(2 * M_PI * tn.f * tt));
                    if (tn.side != 'R') buf[i * 2] += v;
                    if (tn.side != 'L') buf[i * 2 + 1] += v;
                }
            }
            if (!wav_in.empty() && now >= in_wav_t) {
                int nw = (int)(wav_in_rate * sl + 0.5);
                if (wav_in_pos < wav_in.size()) {
                    size_t take = std::min((size_t)nw * 2, wav_in.size() - wav_in_pos);
                    m.feed_input(&wav_in[wav_in_pos], take / 2, wav_in_rate);
                    wav_in_pos += take;
                }
            } else if (any) m.feed_input(buf.data(), nfr, 44100);
        }
        for (auto &pe : patchev) if (pe.first >= 0 && pe.first <= now) { m.set_patch_select(pe.second.first, pe.second.second != 0); pe.first = -1; }
        for (auto &a2 : analog_at) if (a2.first >= 0 && a2.first <= now) { m.analog[a2.second.first] = a2.second.second; a2.first = -1; }
        for (auto &r : raws) if (r.first >= 0 && r.first <= now) { m.duart.rx_byte(1, r.second.first); m.duart.rx_byte(1, r.second.second); r.first = -1; }
        m.run(slice);
        std::string d0 = m.panel.text(), d1 = m.panel.attr_line(0), dc = m.panel.attr_line(1);
        for (int k = 0; k < Panel::COLS; k++) if (dc[k] != ' ') d1[k] = dc[k];
        std::string inds; for (int i = 0; i < 16; i++) inds += " *B"[m.panel.ind[i] % 3];
        d1 += " [" + inds + "]";
        { std::string at = "A:"; at += m.audio_led(0) ? 'Y' : '-'; at += m.audio_led(2) ? 'R' : '-';
          at += " B:"; at += m.audio_led(1) ? 'Y' : '-'; at += m.audio_led(3) ? 'R' : '-'; d1 += " {" + at + "}"; }
        d1 += " <" + m.panel.annunciators() + ">";
        if (d0 != last0 || d1 != last1) {
            fprintf(stderr, "[%7.3fs] |%s|\n           |%s|\n", now, d0.c_str(), d1.c_str());
            last0 = d0; last1 = d1;
        }
    }
    fprintf(stderr, "end pc=%06x\n", m.last_pc());
    fprintf(stderr, "FINAL |%s|\n", m.panel.text().c_str());
    if (pc_hist) {
        std::vector<std::pair<uint64_t,uint32_t>> v; for (auto &x : hist) v.push_back({x.second, x.first});
        std::sort(v.rbegin(), v.rend());
        for (size_t i = 0; i < v.size() && i < 25; i++) fprintf(stderr, "  %06x: %llu\n", v[i].second, (unsigned long long)v[i].first);
    }
    for (int k = 0; k < dis_n; k++) {
        char buf[128]; int l = m68k_disassemble(buf, dis_addr, M68K_CPU_TYPE_68000);
        fprintf(stderr, "%06X: %s\n", dis_addr, buf); dis_addr += l;
    }
    for (int k = 0; k < hex_len; k += 16) {
        fprintf(stderr, "%06X:", hex_addr + k);
        for (int j = 0; j < 16 && k + j < hex_len; j++) fprintf(stderr, " %02x", m.read(hex_addr + k + j, 1));
        fprintf(stderr, "\n");
    }
    if (esp_list) m.esp.list_program(stderr);
    if (otto_dump) m.otto.dump(stderr);
    if (const char *f = getenv("ASR_DUMP_HI")) {   // the 512 KB at $F80000 (OS code), through the CPU map
        FILE *o = fopen(f, "wb");
        for (uint32_t a = 0xf80000; a < 0x1000000; a++) { uint8_t b = (uint8_t)m.read(a, 1); fwrite(&b, 1, 1, o); }
        fclose(o);
    }
    if (!dump_ram.empty()) { FILE *f = fopen(dump_ram.c_str(), "wb"); fwrite(m.ram.data(), 1, m.ram.size(), f); fclose(f); }
    if (write_back && m.fdc.dirty) { if (m.fdc.save()) fprintf(stderr, "disk image written back\n"); }
    if (!wav.empty()) write_wav(wav.c_str(), m.audio, (int)m.out_rate);
    if (!aux_wav.empty()) {                 // AUX1-3 (Output Expander) summed to one stereo file
        std::vector<float> st(m.audio_aux.size() / 3);
        for (size_t i = 0; i + 5 < m.audio_aux.size(); i += 6) {
            st[i / 3] = m.audio_aux[i] + m.audio_aux[i + 2] + m.audio_aux[i + 4];
            st[i / 3 + 1] = m.audio_aux[i + 1] + m.audio_aux[i + 3] + m.audio_aux[i + 5];
        }
        write_wav(aux_wav.c_str(), st, (int)m.out_rate);
    }
    return 0;
}
