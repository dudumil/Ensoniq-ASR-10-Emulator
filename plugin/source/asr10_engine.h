// The emulated ASR-10 inside the plug-in: owns one Asr10 machine and runs it in step with the host.
//
// The 68000 core (Musashi) keeps its state in globals, so several plug-in instances share it:
// every access to a machine goes through one process-wide lock and the CPU context of the
// instance that wants to run is swapped in first (activate()).
#pragma once
#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include "asr10.h"

struct EngineMidi { int offset; uint8_t bytes[3]; int len; };

// what the editor needs to draw the front panel
struct EngineSnapshot {
    Panel panel;
    bool audio_led[4] = {};
    bool disk = false, drive_busy = false;
    float in_left = 0, in_right = 0;
    std::string floppy, scsi;            // floppy image path, SCSI summary
    std::string status;                  // last message for the status line
    uint32_t status_serial = 0;
    bool booted = false;
};

class Asr10Engine {
public:
    Asr10Engine();
    ~Asr10Engine();

    // resources = folder with the ROMs (and os353.img), user = writable folder for disk images
    bool boot(const std::string &resources, const std::string &user);
    bool booted() const { return ok; }
    const std::string &error() const { return err; }
    void message(const std::string &s) { set_status(s); }   // a line for the editor

    void set_sample_rate(double fs);
    // out[0..7] = main L/R, aux1 L/R, aux2 L/R, aux3 L/R (any may be null); in = stereo input or null.
    // aux_active[i] = AUX i+1 output bus is active in the host (it is then left out of the main mix)
    void process(float *const *out, const float *in_l, const float *in_r, int frames,
                 const std::vector<EngineMidi> &midi, const bool aux_active[3]);
    std::vector<uint8_t> take_midi_out();          // bytes sent by the ASR (audio thread)

    // ----- control (any thread; queued and applied by process(), or directly when not processing)
    void button(int idx, bool down);               // panel button (RackUI index / AUDIO_A / AUDIO_B)
    void analog(int ch, uint16_t v);
    void insert_disk(const std::string &path);     // floppy (or SCSI hard disk when > 2 MB)
    void insert_files(const std::vector<std::string> &files);   // .efe/.efa files: put on a temporary floppy
    void eject_disk();
    void attach_scsi(const std::string &path);
    void detach_scsi();
    // settings come from the stand-alone program (asr10.cfg); call now and then (editor timer)
    // to pick up changes made there
    void check_settings();
    void power_reset();

    // state saved with the host project
    std::string floppy_path() const;
    std::vector<std::string> scsi_paths() const;
    void restore(const std::string &floppy, const std::vector<std::string> &scsi);

    EngineSnapshot snapshot();
    double emulated_seconds();                     // CPU time of the machine (diagnostics)
    void save_floppy();                            // write back a modified floppy image

private:
    void activate();                               // global lock must be held
    void run_commands();                           // global lock held, machine active
    void ensure_frames(size_t frames);
    void set_status(const std::string &s);
    std::string attach_scsi_folder(Asr10 &a);     // (re)attach the folder's drives + the user's images
    std::string scsi_folder_path() const;          // the folder actually used ("" = none)
    void apply_settings(Asr10 &a, bool first);     // read asr10.cfg and apply it

    std::unique_ptr<Asr10> m;
    std::vector<unsigned char> cpu_ctx;
    bool ok = false;
    std::string err, user_dir, res_dir;
    double fs = 44100;

    std::mutex cmd_mx;
    std::vector<std::function<void(Asr10 &)>> cmds;
    std::vector<uint8_t> midi_out;
    std::string status; uint32_t status_serial = 0;
    std::vector<std::string> scsi_list;            // images attached by the user, by SCSI ID
    std::string floppy;
    std::string scsi_folder;                       // chosen folder, "" = default
    std::string app_dir;                           // the stand-alone program's folder (from the .cfg)
    std::filesystem::file_time_type cfg_stamp{};
};

std::string asr10_module_dir();                    // folder of the plug-in binary
std::string asr10_user_dir();                      // per-user data folder (created)
