#include "u8file.h"
#include "fdc.h"
#include "ede_image.h"
#include <cstdio>
#include <cstring>

void Fdc::reset() {
    phase = CMD; cmd_n = 0; res_n = res_pos = 0;
    drq = false; set_irq(false); seek_int = false;
    byte_ready = false; exec_delay = 0;
}

bool Fdc::load(const std::string &p) {
    FILE *f = u8fopen(p.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    disk.resize(sz);
    if (fread(disk.data(), 1, sz, f) != (size_t)sz) { fclose(f); disk.clear(); return false; }
    fclose(f);
    {   // a Giebler .EDE / .EDA image: work on the raw disk, write the same format back
        std::vector<uint8_t> raw;
        if (ede::decode(disk, raw, ede_header)) disk.swap(raw); else ede_header.clear();
    }
    path = p; dirty = false;
    geometry();
    changed = true;
    ready_change();
    return true;
}

// The uPD72069 polls the drive READY line while idle; a change raises INT with
// ST0 = abnormal termination due to ready change (IC = 11).
void Fdc::ready_change() {
    // The ASR-10 OS does not expect ready-change interrupts (it crashes on one), so
    // polling is evidently disabled on the real unit; disk presence is detected via
    // SENSE DRIVE STATUS (ST3 RDY) and the DUART IP0 READY input instead.
}

bool Fdc::save() {
    if (path.empty() || !dirty) return true;
    FILE *f = u8fopen(path.c_str(), "wb");
    if (!f) return false;
    std::vector<uint8_t> packed;
    if (!ede_header.empty()) packed = ede::encode(disk, ede_header);
    const std::vector<uint8_t> &out = packed.empty() ? disk : packed;
    fwrite(out.data(), 1, out.size(), f); fclose(f);
    dirty = false;
    return true;
}

void Fdc::geometry() {
    tracks = 80; heads = 2; ssize = 512;
    if (disk.size() == 1638400) spt = 20;        // HD 1.6MB Ensoniq
    else if (disk.size() == 819200) spt = 10;    // DD 800KB Ensoniq
    else if (disk.size() == 1474560) spt = 18;   // PC HD
    else if (disk.size() == 737280) spt = 9;     // PC DD
    else spt = (int)(disk.size() / (tracks * heads * ssize));
}

long Fdc::sector_offset(int cyl, int head, int sec) {
    if (disk.empty() || cyl >= tracks || head >= heads || sec < 0 || sec >= spt) return -1;
    return ((long)(cyl * heads + head) * spt + sec) * ssize;
}

int Fdc::cmd_length(uint8_t c) {
    switch (c & 0x1f) {
    case 0x02: return 9;  // read track
    case 0x03: return 3;  // specify
    case 0x04: return 2;  // sense drive status
    case 0x05: return 9;  // write data
    case 0x06: return 9;  // read data
    case 0x07: return 2;  // recalibrate
    case 0x08: return 1;  // sense interrupt status
    case 0x09: return 9;  // write deleted
    case 0x0a: return 2;  // read id
    case 0x0c: return 9;  // read deleted
    case 0x0d: return 6;  // format track
    case 0x0f: return 3;  // seek
    case 0x10: return 1;  // version
    case 0x11: return 9;  // scan equal
    case 0x13: return 4;  // configure
    case 0x19: return 9;  // scan low or equal
    case 0x1d: return 9;  // scan high or equal
    }
    return 1;
}

uint8_t Fdc::read(int a) {
    if (a == 0) {
        uint8_t msr = 0;
        switch (phase) {
        case CMD: msr = 0x80; break;
        case RESULT: msr = 0xd0; break;   // RQM | DIO | CB
        case EXEC:
            msr = 0x10;
            if (nd) { msr |= 0x20; if (byte_ready) msr |= 0x80 | (exec_write ? 0 : 0x40); }
            break;
        }
        return msr;
    }
    // data register
    if (phase == RESULT) {
        uint8_t v = res[res_pos++];
        if (res_pos >= res_n) { phase = CMD; set_irq(false); }
        return v;
    }
    if (phase == EXEC && nd && !exec_write) return dma_read();
    return 0xff;
}

void Fdc::write(int a, uint8_t v) {
    if (a == 0) {
        // uPD72069 auxiliary commands
        if (log) fprintf(stderr, "[FDC] aux cmd %02x\n", v);
        if (v == 0x36) { reset(); return; }
        if (v == 0x35) return;   // issued without reading a result (used when switching to SCSI)
        // all other uPD72069 auxiliary commands (motor enable, internal mode,
        // data rate, format select, standby, clock...) answer with ST0 = $80
        if ((v & 0x0f) == 0x0e) motor = (v >> 4) & 0x0f;
        res[0] = 0x80; res_n = 1; res_pos = 0; phase = RESULT;
        return;
    }
    if (phase == EXEC && nd && exec_write) { dma_write(v); return; }
    if (phase != CMD) return;
    if (cmd_n == 0) { cmd_len = cmd_length(v); set_irq(false); }
    cmd[cmd_n++] = v;
    if (cmd_n >= cmd_len) { cmd_n = 0; start_command(); }
}

void Fdc::finish(uint8_t s0, uint8_t s1, uint8_t s2) {
    drq = false;
    res[0] = s0; res[1] = s1; res[2] = s2;
    res[3] = c; res[4] = h; res[5] = r; res[6] = n;
    res_n = 7; res_pos = 0; phase = RESULT;
    set_irq(true);
}

void Fdc::start_command() {
    uint8_t op = cmd[0] & 0x1f;
    int drv = cmd[1] & 3;
    int head = (cmd[1] >> 2) & 1;
    if (log) {
        fprintf(stderr, "[FDC] cmd");
        for (int i = 0; i < cmd_len; i++) fprintf(stderr, " %02x", cmd[i]);
        fprintf(stderr, "\n");
    }
    switch (op) {
    case 0x03: nd = cmd[2] & 1; phase = CMD; return;
    case 0x04: {
        st3 = drv | (head << 2) | 0x08;          // two-side
        if (!disk.empty()) st3 |= 0x20;           // drive READY only with a disk inserted
        if (pcn == 0) st3 |= 0x10;
        if (write_protect || disk.empty()) st3 |= 0x40;
        res[0] = st3; res_n = 1; res_pos = 0; phase = RESULT; return;
    }
    case 0x07:
        pcn = 0; seek_int = true; seek_st0 = 0x20 | drv;
        phase = CMD; set_irq(true); return;
    case 0x0f:
        pcn = cmd[2]; if (pcn >= tracks) pcn = tracks - 1;
        seek_int = true; seek_st0 = 0x20 | drv | (head << 2);
        phase = CMD; set_irq(true); return;
    case 0x08:
        if (seek_int) { res[0] = seek_st0; res[1] = pcn; res_n = 2; seek_int = false; }
        else { res[0] = 0x80; res_n = 1; }
        res_pos = 0; phase = RESULT; set_irq(false);
        return;
    case 0x10: res[0] = 0x90; res_n = 1; res_pos = 0; phase = RESULT; return;
    case 0x13: phase = CMD; return;
    case 0x0a: // read id
        c = pcn; h = head; r = 0; n = 2;
        if (disk.empty()) { finish(0x40 | drv | (head << 2), 0x01, 0); return; }
        finish(drv | (head << 2), 0, 0); return;
    case 0x06: case 0x0c: case 0x05: case 0x09: case 0x02: {
        c = cmd[2]; h = cmd[3]; r = cmd[4]; n = cmd[5]; eot = cmd[6];
        mt = (cmd[0] & 0x80) != 0;
        exec_write = (op == 0x05 || op == 0x09);
        side_sel = head;
        if (disk.empty()) { finish(0x48 | drv | (head << 2), 0x00, 0); return; }   // not ready
        if (exec_write && write_protect) { finish(0x40 | drv | (head << 2), 0x02, 0); return; }
        long off = sector_offset(pcn, head, r);
        if (off < 0 || c != pcn) { finish(0x40 | drv | (head << 2), 0x04, c != pcn ? 0x10 : 0); return; }
        secbuf.assign(disk.begin() + off, disk.begin() + off + ssize);
        byte_pos = 0; phase = EXEC; byte_ready = false; byte_timer = 0; exec_delay = 2000;
        return;
    }
    case 0x0d: { // format track: accept C,H,R,N per sector via DMA (4 bytes each), fill with data pattern
        exec_write = true; mt = false;
        n = cmd[2]; eot = cmd[3];
        c = pcn; h = head; r = 0;
        secbuf.assign(4, 0); byte_pos = 0; phase = EXEC; byte_ready = false; exec_delay = 2000;
        secbuf.resize(0);
        cmd[8] = cmd[5];   // filler byte
        return;
    }
    default:
        res[0] = 0x80; res_n = 1; res_pos = 0; phase = RESULT; return;
    }
}

bool Fdc::next_sector() {
    // advance to the next sector of a multi-sector transfer; return false at end of track
    if (r == eot) {
        if (mt && h == 0) { h = 1; r = 0; side_sel = 1; }
        else { c++; r = 0; return false; }
    } else r++;
    long off = sector_offset(pcn, side_sel, r);
    if (off < 0) return false;
    if (!exec_write) secbuf.assign(disk.begin() + off, disk.begin() + off + ssize);
    byte_pos = 0;
    return true;
}

uint8_t Fdc::dma_read() {
    if (phase != EXEC || exec_write || !byte_ready) return 0xff;
    uint8_t v = secbuf[byte_pos++];
    byte_ready = false; drq = false;
    if (byte_pos >= ssize) {
        if (!next_sector()) {
            // end of cylinder without TC
            finish(0x40 | (cmd[1] & 7), 0x80, 0);
        }
    }
    return v;
}

void Fdc::dma_write(uint8_t v) {
    if (phase != EXEC || !exec_write || !byte_ready) return;
    byte_ready = false; drq = false;
    if ((cmd[0] & 0x1f) == 0x0d) {
        secbuf.push_back(v);
        if (secbuf.size() == 4) {
            int fr = secbuf[2];
            long off = sector_offset(pcn, cmd[1] >> 2 & 1, fr);
            if (off >= 0) { memset(&disk[off], cmd[8], ssize); dirty = true; }
            secbuf.clear();
            if (++byte_pos >= eot) { r = fr; finish(cmd[1] & 7, 0, 0); }
        }
        return;
    }
    secbuf[byte_pos++] = v;
    if (byte_pos >= ssize) {
        long off = sector_offset(pcn, side_sel, r);
        if (off >= 0) { memcpy(&disk[off], secbuf.data(), ssize); dirty = true; }
        if (!next_sector()) finish(0x40 | (cmd[1] & 7), 0x80, 0);
    }
}

void Fdc::tc() {
    if (phase != EXEC) return;
    if (exec_write && (cmd[0] & 0x1f) != 0x0d && byte_pos > 0) {
        // flush partial sector
        long off = sector_offset(pcn, side_sel, r);
        if (off >= 0) { memcpy(&disk[off], secbuf.data(), ssize); dirty = true; }
    }
    // the result reports the sector after the last one transferred
    if (byte_pos >= ssize || byte_pos == 0) { /* already advanced */ }
    else { if (r == eot) { r = 0; c++; } else r++; }
    finish(cmd[1] & 7, 0, 0);
}

void Fdc::run(int clocks) {
    if (phase != EXEC) return;
    if (exec_delay > 0) { exec_delay -= clocks; if (exec_delay > 0) return; }
    if (byte_ready) return;
    byte_timer -= clocks;
    if (byte_timer > 0) return;
    byte_timer = 32;  // fast transfer (real drive: ~256 clocks/byte at 500 kbit/s)
    byte_ready = true;
    if (!nd) drq = true;
    else set_irq(true);
}
