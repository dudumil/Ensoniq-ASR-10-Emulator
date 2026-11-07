// MC68302 IMP peripheral emulation (interrupt controller, ports, timers,
// chip selects, IDMA, SCC UART mode with buffer descriptors).
// Register layout per Motorola MC68302UM rev 2 (cross-checked with RTEMS m68302.h).
#include "m68302.h"
#include <cstdio>
#include <cstring>

void M68302::reset() {
    bar = 0xbfff; scr = 0x0000000f;
    memset(dpram, 0, sizeof(dpram));
    // CS0 decodes the boot ROM at reset (BR0=$C001, OR0=$DFFD)
    br[0] = 0xc001; opt[0] = 0xdffd;
    for (int i = 1; i < 4; i++) { br[i] = 0; opt[i] = 0; }
    gimr = ipr = imr = isr = 0;
    pacnt = paddr = padat = pbcnt = pbddr = pbdat = 0;
    for (auto &t : tim) t = Timer{};
    wrr = 0xffff; wcn = 0;
    cmr = 0; sapr = dapr = 0; bcr = 0; csr = 0; fcr = 0;
    for (auto &s : scc) s = Scc{};
    spmode = simask = simode = 0;
    cur_ipl = 0;
    dpram[0x67e] = 0; dpram[0x67f] = 2;  // revision B
}

int M68302::chip_select(uint32_t a, bool write) const {
    a &= 0xffffff;
    for (int i = 0; i < 4; i++) {
        if (!(br[i] & 1)) continue;
        uint32_t base = (uint32_t)(br[i] & 0x1ffc) << 11;
        uint32_t mask = (uint32_t)(opt[i] & 0x1ffc) << 11;
        if (i == 0 && opt[0] == 0xdffd && br[0] == 0xc001) {
            // reset state: boot ROM answers the whole space until reprogrammed
            // (FC compare is not modelled; CS0 must cover the reset vectors)
            mask = 0;
        }
        if ((a & mask) != (base & mask)) continue;
        if (opt[i] & 2) {               // compare R/W
            bool wr_only = (br[i] & 2) != 0;   // BR.RW: 0 = read cycles, 1 = write cycles
            if (wr_only != write) continue;
        }
        return i;
    }
    return -1;
}

void M68302::update_irq() {
    // internal sources request level 4
    int lvl = 0;
    if (ext_irq[7]) lvl = 7;
    else if (ext_irq[6]) lvl = 6;
    else if ((ipr & imr) & ~0) {
        // An interrupt is presented if it is not masked and its priority is
        // higher than any currently in service.
        uint16_t pend = ipr & imr;
        int hp = 15; while (hp >= 0 && !(pend & (1 << hp))) hp--;
        int hs = 15; while (hs >= 0 && !(isr & (1 << hs))) hs--;
        if (hp > hs) lvl = 4;
    }
    if (!lvl && ext_irq[1]) lvl = 1;
    if (lvl < 4 && ext_irq[1] && lvl < 1) lvl = 1;
    if (lvl != cur_ipl) { cur_ipl = lvl; if (irq_changed) irq_changed(lvl); }
}

void M68302::set_irq_pin(int level, bool state) {
    if (level < 0 || level > 7) return;
    ext_irq[level] = state;
    update_irq();
}

void M68302::pb_pin(int bit, bool level) {
    uint16_t m = 1u << bit;
    bool old = (pb_last & m) != 0;
    if (level) pb_last |= m; else pb_last &= ~m;
    if (old && !level) {
        // falling edge on PB8..PB11 -> pending
        static const int ipr_bit[4] = { 1, 7, 14, 15 };
        if (bit >= 8 && bit <= 11) raise(ipr_bit[bit - 8]);
    }
}

int M68302::int_ack(int level) {
    int vbase = gimr & 0xe0;
    if (level == 4) {
        uint16_t pend = ipr & imr;
        int hp = 15; while (hp >= 0 && !(pend & (1 << hp))) hp--;
        if (hp < 0) return 24;  // spurious
        ipr &= ~(1u << hp);
        isr |= (1u << hp);
        update_irq();
        return vbase | hp;
    }
    // GIMR.IVx = 0: the IMP supplies the vector; 1: external vector (autovector here)
    if (level == 1) { if (!(gimr & 0x1000)) return vbase | 17; return 25; }
    if (level == 6) { if (!(gimr & 0x2000)) return vbase | 22; return 30; }
    if (level == 7) { if (!(gimr & 0x4000)) return vbase | 23; return 31; }
    return 24 + level;
}

uint8_t M68302::sysreg_read8(uint32_t a) {
    switch (a & 0xff) {
    case 0xf2: return bar >> 8;
    case 0xf3: return bar;
    case 0xf4: return scr >> 24;
    case 0xf5: return scr >> 16;
    case 0xf6: return scr >> 8;
    case 0xf7: return scr;
    }
    return 0xff;
}
void M68302::sysreg_write8(uint32_t a, uint8_t v) {
    switch (a & 0xff) {
    case 0xf2: bar = (bar & 0x00ff) | (v << 8); break;
    case 0xf3: bar = (bar & 0xff00) | v; break;
    case 0xf4: scr = (scr & 0x00ffffff) | ((uint32_t)v << 24); break;
    case 0xf5: scr = (scr & 0xff00ffff) | ((uint32_t)v << 16); break;
    case 0xf6: scr = (scr & 0xffff00ff) | ((uint32_t)v << 8); break;
    case 0xf7: scr = (scr & 0xffffff00) | v; break;
    }
}

uint8_t M68302::read8(uint32_t a) {
    uint32_t off = a & 0xfff;
    if (off < 0x800) return dpram[off];
    uint16_t w = reg_read16(off & ~1);
    return (off & 1) ? (w & 0xff) : (w >> 8);
}
uint16_t M68302::read16(uint32_t a) {
    uint32_t off = a & 0xffe;
    if (off < 0x800) return rd16(off);
    return reg_read16(off);
}
void M68302::write8(uint32_t a, uint8_t v) {
    uint32_t off = a & 0xfff;
    if (off < 0x800) { dpram[off] = v; return; }
    if (off & 1) reg_write16(off & ~1, v, 0x00ff);
    else reg_write16(off, v << 8, 0xff00);
}
void M68302::write16(uint32_t a, uint16_t v) {
    uint32_t off = a & 0xffe;
    if (off < 0x800) { wr16(off, v); return; }
    reg_write16(off, v, 0xffff);
}

uint16_t M68302::reg_read16(uint32_t off) {
    switch (off) {
    case 0x802: return cmr;
    case 0x804: return sapr >> 16;
    case 0x806: return sapr;
    case 0x808: return dapr >> 16;
    case 0x80a: return dapr;
    case 0x80c: return bcr;
    case 0x80e: return csr << 8;
    case 0x810: return fcr << 8;
    case 0x812: return gimr;
    case 0x814: return ipr;
    case 0x816: return imr;
    case 0x818: return isr;
    case 0x81e: return pacnt;
    case 0x820: return paddr;
    case 0x822: {
        uint16_t ext = porta_in ? porta_in() : 0xffff;
        return (padat & paddr) | (ext & ~paddr);
    }
    case 0x824: return pbcnt;
    case 0x826: return pbddr;
    case 0x828: {
        uint16_t ext = portb_in ? portb_in() : 0xffff;
        // PB8-11 are inputs by default unless direction set
        return (pbdat & pbddr) | (ext & ~pbddr);
    }
    case 0x830: case 0x834: case 0x838: case 0x83c: return br[(off - 0x830) >> 2];
    case 0x832: case 0x836: case 0x83a: case 0x83e: return opt[(off - 0x830) >> 2];
    case 0x840: return tim[0].tmr;
    case 0x842: return tim[0].trr;
    case 0x844: return tim[0].tcr;
    case 0x846: return tim[0].tcn;
    case 0x848: return tim[0].ter;
    case 0x84a: return wrr;
    case 0x84c: return wcn;
    case 0x850: return tim[1].tmr;
    case 0x852: return tim[1].trr;
    case 0x854: return tim[1].tcr;
    case 0x856: return tim[1].tcn;
    case 0x858: return tim[1].ter;
    case 0x860: return 0;   // CR: FLG always clear (commands complete instantly)
    case 0x8b0: return spmode;
    case 0x8b2: return simask;
    case 0x8b4: return simode;
    }
    if (off >= 0x880 && off < 0x8b0) {
        Scc &s = scc[(off - 0x880) >> 4];
        switch (off & 0xf) {
        case 0x2: return s.scon;
        case 0x4: return s.scm;
        case 0x6: return s.dsr;
        case 0x8: return s.scce << 8;
        case 0xa: return s.sccm << 8;
        case 0xc: return s.sccs << 8;
        }
        return 0;
    }
    if (log) fprintf(stderr, "[68302] unhandled reg read %03x\n", off);
    return 0;
}

#define MERGE(r) r = (uint16_t)((r & ~mask) | (v & mask))

void M68302::reg_write16(uint32_t off, uint16_t v, uint16_t mask) {
    switch (off) {
    case 0x802: MERGE(cmr);
        if (dbg_scc && (cmr & 1)) fprintf(stderr, "[IDMA] start cmr=%04x sapr=%06x dapr=%06x bcr=%04x\n", cmr, sapr, dapr, bcr);
        if (cmr & 1) idma_run(); return;
    case 0x804: sapr = (sapr & ~((uint32_t)mask << 16)) | ((uint32_t)(v & mask) << 16); return;
    case 0x806: sapr = (sapr & ~(uint32_t)mask) | (v & mask); return;
    case 0x808: dapr = (dapr & ~((uint32_t)mask << 16)) | ((uint32_t)(v & mask) << 16); return;
    case 0x80a: dapr = (dapr & ~(uint32_t)mask) | (v & mask); return;
    case 0x80c: MERGE(bcr); return;
    case 0x80e: if (mask & 0xff00) csr &= ~(v >> 8); return;   // write 1 to clear
    case 0x810: if (mask & 0xff00) fcr = v >> 8; return;
    case 0x812: MERGE(gimr); update_irq(); return;
    case 0x814: ipr &= ~(v & mask); update_irq(); return;     // write 1 to clear
    case 0x816: MERGE(imr); update_irq(); return;
    case 0x818: isr &= ~(v & mask); update_irq(); return;     // write 1 to clear
    case 0x81e: MERGE(pacnt); return;
    case 0x820: MERGE(paddr); if (porta_out) porta_out(padat, paddr); return;
    case 0x822: MERGE(padat); if (porta_out) porta_out(padat, paddr); return;
    case 0x824: MERGE(pbcnt); return;
    case 0x826: MERGE(pbddr); if (portb_out) portb_out(pbdat, pbddr); return;
    case 0x828: MERGE(pbdat); if (portb_out) portb_out(pbdat, pbddr); return;
    case 0x830: case 0x834: case 0x838: case 0x83c: MERGE(br[(off - 0x830) >> 2]); return;
    case 0x832: case 0x836: case 0x83a: case 0x83e: MERGE(opt[(off - 0x830) >> 2]); return;
    case 0x840: MERGE(tim[0].tmr); if (!(tim[0].tmr & 1)) { tim[0].tcn = 0; tim[0].pre = 0; } return;
    case 0x842: MERGE(tim[0].trr); return;
    case 0x844: MERGE(tim[0].tcr); return;
    case 0x846: tim[0].tcn = 0; return;   // write resets counter
    case 0x848: tim[0].ter &= ~(v & mask & 0xff); return;
    case 0x84a: MERGE(wrr); return;
    case 0x84c: wcn = 0; return;
    case 0x850: MERGE(tim[1].tmr); if (!(tim[1].tmr & 1)) { tim[1].tcn = 0; tim[1].pre = 0; } return;
    case 0x852: MERGE(tim[1].trr); return;
    case 0x854: MERGE(tim[1].tcr); return;
    case 0x856: tim[1].tcn = 0; return;
    case 0x858: tim[1].ter &= ~(v & mask & 0xff); return;
    case 0x860:
        if (mask & 0xff00) {
            uint8_t cr = v >> 8;
            if (cr & 0x80) { // CP reset
                for (auto &s : scc) { s.rxbd = s.txbd = 0; s.rxpos = 0; s.rx_open = false; s.tx_bytepos = 0; s.tx_timer = 0; }
            } else if (cr & 1) {
                scc_command((cr >> 1) & 3, (cr >> 4) & 3);
            }
        }
        return;
    case 0x8b0: MERGE(spmode); return;
    case 0x8b2: MERGE(simask); return;
    case 0x8b4: MERGE(simode); return;
    }
    if (off >= 0x880 && off < 0x8b0) {
        int n = (off - 0x880) >> 4;
        if (dbg_scc) fprintf(stderr, "[SCC%d] reg %03x = %04x/%04x\n", n + 1, off, v, mask);
        Scc &s = scc[n];
        switch (off & 0xf) {
        case 0x2: MERGE(s.scon); return;
        case 0x4: {
            uint16_t old = s.scm; MERGE(s.scm);
            if ((s.scm & 8) && !(old & 8)) { s.rxpos = 0; s.rx_open = false; }
            if (!(s.scm & 8) && (old & 8) && (s.scm & 3) == 3) { s.rxpos = 0; s.rx_open = false; }
            return;
        }
        case 0x6: MERGE(s.dsr); return;
        case 0x8: if (mask & 0xff00) { s.scce &= ~(v >> 8); bool any = false; for (int i = 0; i < 3; i++) any |= false; }
                  // recompute SCC interrupt
                  {
                      static const int bitn[3] = { 13, 10, 8 };
                      if (!(s.scce & s.sccm)) { ipr &= ~(1u << bitn[n]); update_irq(); }
                  }
                  return;
        case 0xa: if (mask & 0xff00) s.sccm = v >> 8;
                  {
                      static const int bitn[3] = { 13, 10, 8 };
                      if (s.scce & s.sccm) raise(bitn[n]);
                  }
                  return;
        case 0xc: return;
        }
        return;
    }
    if (log) fprintf(stderr, "[68302] unhandled reg write %03x = %04x/%04x\n", off, v, mask);
}

void M68302::scc_command(int ch, int op) {
    // op: 0 STOP TRANSMIT, 1 RESTART TRANSMIT, 2 ENTER HUNT MODE, 3 RESET BCS / other
    Scc &s = scc[ch];
    if (op == 1) { /* restart: continue from current BD */ }
    if (op == 0) { s.tx_timer = 0; s.tx_bytepos = 0; }
    if (op == 2) { /* enter hunt: close nothing */ }
}

// ---------------------------------------------------------------- timers
void M68302::timer_tick(int n, int clocks) {
    Timer &t = tim[n];
    if (!(t.tmr & 1)) return;
    int iclk = (t.tmr >> 1) & 3;
    if (iclk == 0 || iclk == 3) return;
    double div = (double)((t.tmr >> 8) + 1) * (iclk == 2 ? 16 : 1);
    t.pre += clocks;
    while (t.pre >= div) {
        t.pre -= div;
        if (t.tcn == t.trr) {
            t.ter |= 2;
            if (t.tmr & 0x10) raise(n == 0 ? 9 : 6);
            if (t.tmr & 8) { t.tcn = 0; continue; }
        }
        t.tcn++;
    }
}

// ---------------------------------------------------------------- IDMA
// Only the external-request byte transfer used by the floppy is modelled:
// each DREQ moves one byte (source/destination incremented per CMR SAPI/DAPI).
void M68302::idma_run() {
    // executed lazily from run()
}

// ---------------------------------------------------------------- SCC
double M68302::scc_byte_clocks(int n) const {
    const Scc &s = scc[n];
    // baud = clk / ((CD+1) * (DIV4?4:1)) / 16 ; ~10 bits per char
    int cd = (s.scon >> 1) & 0x7ff;
    int div4 = (s.scon & 1) ? 4 : 1;
    double clk_per_bit = (double)(cd + 1) * div4 * 16.0;
    if (s.scon & 0x4000) clk_per_bit = 512;  // external clock: 31250 baud at 16 MHz (MIDI)
    return clk_per_bit * 10.0;
}

void M68302::scc_rx(int n, uint8_t b) { scc[n].rxfifo.push_back(b); }

bool M68302::scc_rx_ready(int n) const {
    uint32_t bdbase = 0x400 + n * 0x100;
    const Scc &s = scc[n];
    return (rd16(bdbase + s.rxbd * 8) & 0x8000) != 0;
}

void M68302::scc_tx_service(int n, int clocks) {
    Scc &s = scc[n];
    if (!(s.scm & 4)) return;     // transmitter disabled
    static const int bitn[3] = { 13, 10, 8 };
    uint32_t bdbase = 0x400 + n * 0x100 + 0x40;
    if (s.tx_timer > 0) { s.tx_timer -= clocks; if (s.tx_timer > 0) return; }
    for (int guard = 0; guard < 8; guard++) {
        uint32_t bd = bdbase + s.txbd * 8;
        uint16_t st = rd16(bd);
        if (!(st & 0x8000)) return;     // not ready
        uint16_t len = rd16(bd + 2);
        uint32_t ptr = rd32(bd + 4) & 0xffffff;
        if (s.tx_bytepos < len) {
            uint8_t b = mem_read8(ptr + s.tx_bytepos);
            s.tx_bytepos++;
            if (scc_tx) scc_tx(n, b);
            s.tx_timer += scc_byte_clocks(n);
            if (s.tx_bytepos < len) return;
        }
        // buffer done
        s.tx_bytepos = 0;
        wr16(bd, st & ~0x8000);
        if (st & 0x1000) { s.scce |= 0x02; if (s.sccm & 0x02) raise(bitn[n]); }
        s.txbd = (st & 0x2000) ? 0 : (s.txbd + 1) & 7;
        if (s.tx_timer > 0) return;
    }
}

// Synchronous receive (BISYNC mode, external clock and sync): used by the ASR-10 for the
// audio A/D stream. The bytes go straight into the current Rx buffer; a buffer is closed
// when MRBLR bytes have been received.
bool M68302::scc_sync_active(int n) const {
    const Scc &s = scc[n];
    return (s.scm & 3) == 3 && (s.scm & 8);
}

void M68302::scc_sync_rx(int n, uint8_t b) {
    Scc &s = scc[n];
    if (!scc_sync_active(n)) { s.rxpos = 0; s.rx_open = false; return; }
    static const int bitn[3] = { 13, 10, 8 };
    uint32_t base = 0x400 + n * 0x100;
    uint32_t bd = base + s.rxbd * 8;
    uint16_t st = rd16(bd);
    if (!(st & 0x8000)) {                       // no empty buffer: busy, byte lost
        if (dbg_scc && !(s.scce & 0x04)) fprintf(stderr, "[SCC%d] sync rx busy bd%d st=%04x\n", n + 1, s.rxbd, st);
        if (!(s.scce & 0x04)) { s.scce |= 0x04; if (s.sccm & 0x04) raise(bitn[n]); }
        return;
    }
    // The receiver keeps its position in the parameter RAM: Rx internal data pointer (+$88,
    // long) and Rx internal byte count (+$8C, bytes left in the buffer). The OS reads the
    // pointer to find the punch-in point of an Audio Track recording, and shortens the count
    // to line up the left (SCC1) and right (SCC2) buffers, so both are kept live here.
    if (!s.rx_open) {
        uint16_t mrblr = rd16(base + 0x82);
        if (mrblr == 0) mrblr = 1;
        s.rx_start = rd32(bd + 4) & 0xffffff;
        wr16(base + 0x88, s.rx_start >> 16); wr16(base + 0x8a, s.rx_start & 0xffff);
        wr16(base + 0x8c, mrblr);
        s.rx_open = true;
    }
    uint32_t ptr = rd32(base + 0x88) & 0xffffff;
    uint16_t left = rd16(base + 0x8c);
    mem_write8(ptr, b);
    ptr++;
    if (left) left--;
    wr16(base + 0x88, ptr >> 16); wr16(base + 0x8a, ptr & 0xffff);
    wr16(base + 0x8c, left);
    s.rxpos = (int)(ptr - s.rx_start);
    if (left == 0) {
        wr16(bd + 2, (uint16_t)s.rxpos);
        wr16(bd, st & ~0x8000);
        if (dbg_scc) fprintf(stderr, "[SCC%d] sync rx bd%d done st=%04x ptr=%06x len=%d sccm=%02x\n", n + 1, s.rxbd, st, s.rx_start, s.rxpos, s.sccm);
        s.rxpos = 0; s.rx_open = false;
        s.rxbd = (st & 0x2000) ? 0 : (s.rxbd + 1) & 7;
        if (st & 0x1000) { s.scce |= 0x01; if (s.sccm & 0x01) raise(bitn[n]); }
    }
}

void M68302::scc_rx_service(int n, int clocks) {
    Scc &s = scc[n];
    if ((s.scm & 3) == 3) return;               // BISYNC: fed synchronously by scc_sync_rx
    static const int bitn[3] = { 13, 10, 8 };
    uint32_t base = 0x400 + n * 0x100;
    double bc = scc_byte_clocks(n);
    s.rx_timer -= clocks;
    if (s.rx_timer > 0) return;
    if (!(s.scm & 8)) { s.rxfifo.clear(); s.rx_timer = bc; return; }
    uint32_t bd = base + s.rxbd * 8;
    uint16_t st = rd16(bd);
    if (!s.rxfifo.empty()) {
        if (!(st & 0x8000)) {           // no buffer: busy event, byte lost
            s.rxfifo.pop_front();
            s.scce |= 0x04; if (s.sccm & 0x04) raise(bitn[n]);
            s.rx_timer = bc;
            return;
        }
        uint8_t b = s.rxfifo.front(); s.rxfifo.pop_front();
        uint32_t ptr = rd32(bd + 4) & 0xffffff;
        uint16_t mrblr = rd16(base + 0x82);
        // control character table check
        bool ctrl = false;
        for (int i = 0; i < 8; i++) {
            uint16_t c = rd16(base + 0xb0 + i * 2);
            if (c & 0x8000) break;          // end of table
            if ((c & 0xff) == b) {
                if (c & 0x4000) { ctrl = true; }  // reject: char goes to RCCR, not buffer
                else ctrl = true;
                if (c & 0x4000) { wr16(base + 0xae, b); s.scce |= 0x08; if (s.sccm & 0x08) raise(bitn[n]); s.rx_timer = bc; return; }
                break;
            }
        }
        mem_write8(ptr + s.rxpos, b);
        s.rxpos++;
        wr16(bd + 2, s.rxpos);
        s.idle_count = 0;
        if (s.rxpos >= mrblr || ctrl) {
            wr16(bd, (st & ~0x8000) | (ctrl ? 0x0800 : 0));
            s.rxpos = 0; s.rx_open = false;
            s.rxbd = (st & 0x2000) ? 0 : (s.rxbd + 1) & 7;
            if (st & 0x1000) { s.scce |= 0x01; if (s.sccm & 0x01) raise(bitn[n]); }
        }
        s.rx_timer = bc;
        return;
    }
    // idle line: count idle characters and close a partially filled buffer
    s.rx_timer = bc;
    if (s.rxpos > 0 && (st & 0x8000)) {
        uint16_t max_idl = rd16(base + 0x9c);
        s.idle_count++;
        if (max_idl != 0 && s.idle_count >= max_idl) {
            wr16(bd, (st & ~0x8000) | 0x0100);
            s.rxpos = 0; s.rx_open = false; s.idle_count = 0;
            s.rxbd = (st & 0x2000) ? 0 : (s.rxbd + 1) & 7;
            if (st & 0x1000) { s.scce |= 0x01; if (s.sccm & 0x01) raise(bitn[n]); }
        }
    }
}

void M68302::run(int clocks) {
    timer_tick(0, clocks);
    timer_tick(1, clocks);
    for (int n = 0; n < 3; n++) {
        if ((scc[n].scm & 3) == 0 && !(scc[n].scm & 0xc)) continue;
        scc_tx_service(n, clocks);
        scc_rx_service(n, clocks);
    }
    // IDMA with external request (byte transfers, as used for the floppy)
    if ((cmr & 1) && bcr && idma_dreq) {
        int reqg = (cmr >> 10) & 3;   // 0/1 internal, 2 external burst, 3 external cycle steal
        int guard = 64;
        while (bcr && guard-- > 0 && (reqg < 2 || idma_dreq(0))) {
            uint8_t b = mem_read8(sapr & 0xffffff);
            mem_write8(dapr & 0xffffff, b);
            if (cmr & 0x0200) sapr += 1;   // SAPI
            if (cmr & 0x0100) dapr += 1;   // DAPI
            bcr--;
        }
        if (!bcr) {
            csr |= 0x01;          // DONE (CSR bit 0)
            cmr &= ~1;
            if (idma_done && reqg >= 2) idma_done();   // external request = floppy: assert TC
            if (cmr & 0x2000) raise(11);   // INTN
        }
    }
}

