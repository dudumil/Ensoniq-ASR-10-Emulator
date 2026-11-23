// Front-end settings (audio, MIDI), stored as "key=value" lines in asr10.cfg.
#pragma once
#include <string>

struct Settings {
    // audio
    std::string audio_api;             // "" = first available driver
    std::string audio_out;             // device name, "" = default
    std::string audio_in;              // device name, "" = no input
    unsigned rate = 0;                 // 0 = device default
    unsigned buffer = 256;
    int in_left = 0, in_right = 1;     // input channels (0-based) used for the sampler's L/R
    int main_pair = 0;                 // output pair for the main outputs (0 = outs 1-2)
    int aux_pair[3] = { -1, -1, -1 };  // AUX1-3 output pair, -1 = mixed into the main outputs
    // MIDI (port names, "" = none)
    std::string midi_in, midi_out;
    bool midi_in_set = false;          // false: use the first input port, like earlier versions
    // window
    float input_level = 2.0f / 3;      // Input Level pot position 0..1 (2/3 = 0 dB, 1 = +18 dB)
    bool show_keyboard = true;         // strip with the wheels, Patch Select and the keyboard
    // display: false = the original VFD, true = the purple graphic LCD
    bool lcd = false;
    bool dac_tda = true;               // outputs through the TDA1541A converters (else a perfect converter)
    // SCSI drives: folder with ZuluSCSI-style images (HD0.img, CD5.iso ...); "" = the "scsi" folder
    // next to the program
    std::string scsi_dir;
    // the program's own folder, recorded at start so that the VST3 plug-in finds the same files
    std::string app_dir;
    // folder of the ROMs chosen by the user when they were not found ("" = none)
    std::string rom_dir;
    // start-up: false = the OS is loaded from the OS disk image (floppy); true = no floppy at start, the machine
    // boots from a SCSI drive
    bool boot_scsi = false;
    // fast disk access: while a file is loaded or saved (floppy or SCSI, sequencer stopped) the machine runs
    // as fast as the computer allows, without sound (see Asr10::disk_busy)
    bool fast_disk = true;
    // DI-10 Digital I/O board (S/PDIF): the input channel pair of the input device that is the digital
    // input, and the output pair of the output device for the digital output (0-based first channel /
    // pair index, -1 = not used). The board is fitted when either is used.
    int spdif_in = -1, spdif_out = -1;
    bool di10() const { return spdif_in >= 0 || spdif_out >= 0; }

    std::string path;                  // file the settings were loaded from / are saved to
    bool load(const std::string &file);
    bool save() const;
};
