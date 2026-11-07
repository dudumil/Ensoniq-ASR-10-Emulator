// Exploration harness: run the ASR-10 ROM with logging of unknown accesses.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <set>
#include <map>
#include <string>
#include "m68302.h"
extern "C" {
#include "m68k.h"
}

static std::vector<uint8_t> rom;
static std::vector<uint8_t> ram(16 << 20);
static M68302 imp;
static std::set<uint32_t> touched;
static std::map<uint32_t, int> devlog;
static int loglimit = 4000;
static unsigned long long total_cycles = 0;

static uint32_t cur_pc() { return m68k_get_reg(NULL, M68K_REG_PPC); }

static void logacc(const char *what, uint32_t a, uint32_t v, int sz, bool w) {
    if (loglimit-- <= 0) return;
    fprintf(stderr, "%s %c%d %06x = %0*x  pc=%06x cyc=%llu\n", what, w ? 'W' : 'R', sz * 8, a, sz * 2, v, cur_pc(), total_cycles);
}

static uint32_t dev_read(int cs, uint32_t a, int sz) {
    char t[16]; snprintf(t, sizeof t, "CS%d", cs);
    logacc(t, a, 0, sz, false);
    return sz == 1 ? 0xff : 0xffff;
}
static void dev_write(int cs, uint32_t a, uint32_t v, int sz) {
    char t[16]; snprintf(t, sizeof t, "CS%d", cs);
    logacc(t, a, v, sz, true);
}

static uint32_t rd(uint32_t a, int sz) {
    a &= 0xffffff;
    if (a >= 0xf0 && a < 0xf8) {
        uint32_t v = 0; for (int i = 0; i < sz; i++) v = (v << 8) | imp.sysreg_read8(a + i); return v;
    }
    if (imp.is_internal(a)) {
        if (sz == 1) return imp.read8(a);
        if (sz == 2) return imp.read16(a);
        return (imp.read16(a) << 16) | imp.read16(a + 2);
    }
    int cs = imp.chip_select(a, false);
    if (cs == 0) {
        uint32_t o = a & (rom.size() - 1); uint32_t v = 0;
        for (int i = 0; i < sz; i++) v = (v << 8) | rom[(o + i) & (rom.size() - 1)];
        return v;
    }
    if (cs > 0) {
        if (sz == 4) return (dev_read(cs, a, 2) << 16) | dev_read(cs, a + 2, 2);
        return dev_read(cs, a, sz);
    }
    uint32_t pg = a >> 12;
    if (!touched.count(pg)) { touched.insert(pg); fprintf(stderr, "EXT first R%d %06x pc=%06x\n", sz * 8, a, cur_pc()); }
    uint32_t v = 0; for (int i = 0; i < sz; i++) v = (v << 8) | ram[(a + i) & 0xffffff];
    return v;
}
static void wr(uint32_t a, uint32_t v, int sz) {
    a &= 0xffffff;
    if (a >= 0xf0 && a < 0xf8) {
        for (int i = 0; i < sz; i++) imp.sysreg_write8(a + i, v >> (8 * (sz - 1 - i))); return;
    }
    if (imp.is_internal(a)) {
        if (sz == 1) imp.write8(a, v);
        else if (sz == 2) imp.write16(a, v);
        else { imp.write16(a, v >> 16); imp.write16(a + 2, v); }
        return;
    }
    int cs = imp.chip_select(a, true);
    if (cs == 0) { logacc("ROMW", a, v, sz, true); return; }
    if (cs > 0) {
        if (sz == 4) { dev_write(cs, a, v >> 16, 2); dev_write(cs, a + 2, v & 0xffff, 2); }
        else dev_write(cs, a, v, sz);
        return;
    }
    uint32_t pg = a >> 12;
    if (!touched.count(pg)) { touched.insert(pg); fprintf(stderr, "EXT first W%d %06x pc=%06x\n", sz * 8, a, cur_pc()); }
    for (int i = 0; i < sz; i++) ram[(a + i) & 0xffffff] = v >> (8 * (sz - 1 - i));
}

extern "C" {
unsigned int m68k_read_memory_8(unsigned int a) { return rd(a, 1); }
unsigned int m68k_read_memory_16(unsigned int a) { return rd(a, 2); }
unsigned int m68k_read_memory_32(unsigned int a) { return rd(a, 4); }
void m68k_write_memory_8(unsigned int a, unsigned int v) { wr(a, v, 1); }
void m68k_write_memory_16(unsigned int a, unsigned int v) { wr(a, v, 2); }
void m68k_write_memory_32(unsigned int a, unsigned int v) { wr(a, v, 4); }
unsigned int m68k_read_disassembler_16(unsigned int a) { return rd(a, 2); }
unsigned int m68k_read_disassembler_32(unsigned int a) { return rd(a, 4); }
int asr_int_ack(int level) { return imp.int_ack(level); }
void asr_reset_instr(void) {}
static uint32_t trace_from = 0xffffffff; static int trace_n = 0;
void asr_instr_hook(unsigned int pc) {
    if (trace_n > 0 && total_cycles >= trace_from) {
        char buf[128]; m68k_disassemble(buf, pc, M68K_CPU_TYPE_68000);
        fprintf(stderr, "  %06x: %s\n", pc, buf); trace_n--;
    }
}
}

int main(int argc, char **argv) {
    FILE *f = fopen(argc > 1 ? argv[1] : "../roms/asr10_rom.bin", "rb");
    fseek(f, 0, SEEK_END); rom.resize(ftell(f)); rewind(f); fread(rom.data(), 1, rom.size(), f); fclose(f);
    long long maxcyc = argc > 2 ? atoll(argv[2]) : 50000000;
    if (argc > 3) { trace_from = strtoull(argv[3], 0, 10); trace_n = argc > 4 ? atoi(argv[4]) : 2000; }
    imp.reset(); imp.log = true;
    imp.mem_read8 = [](uint32_t a) { return (uint8_t)rd(a, 1); };
    imp.mem_write8 = [](uint32_t a, uint8_t v) { wr(a, v, 1); };
    imp.irq_changed = [](int l) { m68k_set_irq(l); };
    imp.scc_tx = [](int n, uint8_t b) { fprintf(stderr, "SCC%d TX %02x\n", n + 1, b); };
    m68k_init(); m68k_set_cpu_type(M68K_CPU_TYPE_68000); m68k_pulse_reset();
    while (total_cycles < (unsigned long long)maxcyc) {
        int c = m68k_execute(64);
        total_cycles += c;
        imp.run(c);
    }
    fprintf(stderr, "end pc=%06x\n", cur_pc());
}
