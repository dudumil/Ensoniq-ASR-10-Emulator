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
    bool show_keyboard = true;         // strip with the wheels, Patch Select and the keyboard
    // display: false = the original VFD, true = the purple graphic LCD
    bool lcd = false;
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

    std::string path;                  // file the settings were loaded from / are saved to
    bool load(const std::string &file);
    bool save() const;
};
