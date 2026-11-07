// NEC uPD72069 (uPD765 compatible) floppy controller, sector-level emulation,
// with one 3.5" drive holding a raw Ensoniq disk image (.img / .efe-less raw).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

class Fdc {
public:
    void reset();
    uint8_t read(int a);          // a: 0 = MSR, 1 = data
    void    write(int a, uint8_t v);   // a: 0 = aux command (72069), 1 = data
    uint8_t dma_read();
    void    dma_write(uint8_t v);
    void    tc();                 // terminal count from the DMA controller
    bool    dreq() const { return drq; }
    bool    irq() const { return intrq; }
    void    run(int clocks);

    bool load(const std::string &path);
    bool save();
    void eject() { disk.clear(); changed = true; ready_change(); }
    void ready_change();
    bool has_disk() const { return !disk.empty(); }
    bool is_hd() const { return spt >= 18; }
    bool disk_changed() const { return changed; }
    void clear_change() { changed = false; }
    bool write_protect = false;
    int  side_sel = 0;
    int  motor = 0;               // motor enable bits from the 72069 aux command

    std::function<void(bool)> irq_cb;
    bool log = false;
    std::string path;
    bool dirty = false;

private:
    enum Phase { CMD, EXEC, RESULT } phase = CMD;
    uint8_t cmd[9]; int cmd_n = 0, cmd_len = 0;
    uint8_t res[7]; int res_n = 0, res_pos = 0;
    uint8_t st0 = 0, st1 = 0, st2 = 0, st3 = 0;
    int pcn = 0;                  // present cylinder
    bool nd = false;              // non-DMA mode
    bool intrq = false, drq = false;
    bool seek_int = false; uint8_t seek_st0 = 0;
    std::vector<uint8_t> disk;
    int tracks = 80, heads = 2, spt = 20, ssize = 512;
    bool changed = true;

    // execution state for read/write
    bool exec_write = false;
    int c, h, r, n, eot;
    int byte_pos = 0;
    std::vector<uint8_t> secbuf;
    double byte_timer = 0;
    bool  byte_ready = false;
    int   exec_delay = 0;
    bool  mt = false;

    int  cmd_length(uint8_t c);
    void start_command();
    void finish(uint8_t s0, uint8_t s1, uint8_t s2);
    void set_irq(bool s) { intrq = s; if (irq_cb) irq_cb(s); }
    long sector_offset(int c, int h, int r);
    bool next_sector();
    void geometry();
};
