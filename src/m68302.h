// MC68302 Integrated Multiprotocol Processor - on-chip peripherals
// (the 68000 core itself is provided by Musashi)
#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <deque>
#include <vector>

class M68302 {
public:
    // callbacks to the system
    std::function<uint8_t(uint32_t)>        mem_read8;    // used by SDMA/IDMA and SCC buffer access
    std::function<void(uint32_t, uint8_t)>  mem_write8;
    std::function<void(int, uint8_t)>       scc_tx;       // (scc 0..2, byte)
    std::function<void(int)>                irq_changed;  // new 68000 IPL (0..7)
    std::function<uint16_t()>               porta_in;     // external pin state for port A
    std::function<uint16_t()>               portb_in;     // external pin state for port B
    std::function<void(uint16_t,uint16_t)>  porta_out;    // (data, ddr)
    std::function<void(uint16_t,uint16_t)>  portb_out;
    std::function<int(int)>                 idma_dreq;    // returns 1 if external DREQ asserted
    std::function<void()>                   idma_done;    // DONE pulse (-> FDC terminal count)

    void reset();

    // address decode helpers
    bool is_internal(uint32_t a) const {
        uint32_t base = (uint32_t)(bar & 0x0fff) << 12;
        return (bar & 0xf000) != 0xf000 && (a & 0xfff000) == base;  // BAR $Fxxx disables
    }
    // returns chip-select number (0..3) or -1; supervisor/program FC ignored
    int chip_select(uint32_t a, bool write) const;
    int cs_wait(int cs) const { return (opt[cs] >> 13) & 7; }

    uint8_t  read8(uint32_t a);
    void     write8(uint32_t a, uint8_t v);
    uint16_t read16(uint32_t a);
    void     write16(uint32_t a, uint16_t v);

    // system config regs at $F0..$F7
    uint8_t  sysreg_read8(uint32_t a);
    void     sysreg_write8(uint32_t a, uint8_t v);

    // time base: called with elapsed CPU clocks
    void run(int clocks);

    // external interrupt pins (level 1,6,7) - level sensitive by default
    void set_irq_pin(int level, bool state);
    // port B interrupt pins PB8..PB11 (falling edge)
    void pb_pin(int bit, bool level);

    int  int_ack(int level);  // returns vector
    int  ipl() const { return cur_ipl; }

    // serial receive from outside world
    void scc_rx(int n, uint8_t byte);
    bool scc_sync_active(int n) const;          // BISYNC receiver enabled
    void scc_sync_rx(int n, uint8_t byte);      // byte from the synchronous (audio) input
    bool dbg_scc = getenv("ASR_DBG_SCC") != nullptr;
    bool scc_rx_ready(int n) const;     // has an empty Rx BD

    uint16_t bar = 0xbfff;
    uint32_t scr = 0;
    uint8_t  dpram[0x1000];   // whole 4K window (only parts implemented)
    uint16_t br[4], opt[4];

    // diagnostics
    bool log = false;
    uint16_t pbdat_latch = 0;

private:
    // interrupt controller
    uint16_t gimr = 0, ipr = 0, imr = 0, isr = 0;
    bool ext_irq[8] = {};
    int cur_ipl = 0;
    void update_irq();
    void raise(int bit) { ipr |= (1u << bit); update_irq(); }

    // ports
    uint16_t pacnt = 0, paddr = 0, padat = 0, pbcnt = 0, pbddr = 0, pbdat = 0;
    uint16_t pb_last = 0xffff;

    // timers
    struct Timer { uint16_t tmr, trr, tcr, tcn; uint8_t ter; double pre; } tim[2] = {};
    uint16_t wrr = 0, wcn = 0;
    void timer_tick(int n, int clocks);

    // IDMA
    uint16_t cmr = 0; uint32_t sapr = 0, dapr = 0; uint16_t bcr = 0; uint8_t csr = 0, fcr = 0;
    void idma_run();

    // SCCs
    struct Scc {
        uint16_t scon, scm, dsr; uint8_t scce, sccm, sccs;
        int rxbd, txbd;              // current BD index
        int rxpos;                   // bytes in current rx buffer
        bool rx_open;                // sync rx: a buffer is being filled (pointer/count live in parameter RAM)
        uint32_t rx_start;           // sync rx: start address of that buffer
        double tx_timer;             // clocks until current tx byte done
        int    tx_bytepos;
        std::deque<uint8_t> rxfifo;
        double rx_timer;
        int idle_count;
    } scc[3] = {};
    uint16_t spmode = 0, simask = 0, simode = 0;
    void scc_command(int ch, int op);
    void scc_tx_service(int n, int clocks);
    void scc_rx_service(int n, int clocks);
    double scc_byte_clocks(int n) const;
    uint16_t rd16(uint32_t off) const { return (dpram[off & 0xfff] << 8) | dpram[(off + 1) & 0xfff]; }
    uint32_t rd32(uint32_t off) const { return (rd16(off) << 16) | rd16(off + 2); }
    void wr16(uint32_t off, uint16_t v) { dpram[off & 0xfff] = v >> 8; dpram[(off + 1) & 0xfff] = v; }

    uint16_t reg_read16(uint32_t off);
    void     reg_write16(uint32_t off, uint16_t v, uint16_t mask);
};
