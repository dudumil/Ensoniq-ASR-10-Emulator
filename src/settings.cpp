// Front-end settings - see settings.h.
#include "settings.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    size_t i = 0; while (i < s.size() && s[i] == ' ') i++;
    return s.substr(i);
}

bool Settings::load(const std::string &file) {
    path = file;
    FILE *f = fopen(file.c_str(), "r");
    if (!f) return false;
    char line[1024];
    while (fgets(line, sizeof line, f)) {
        std::string l = trim(line);
        if (l.empty() || l[0] == '#') continue;
        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        std::string k = trim(l.substr(0, eq)), v = trim(l.substr(eq + 1));
        int n = atoi(v.c_str());
        if (k == "audio_driver") audio_api = v;
        else if (k == "audio_out") audio_out = v;
        else if (k == "audio_in") audio_in = v;
        else if (k == "sample_rate") rate = (unsigned)n;
        else if (k == "buffer") buffer = (unsigned)n;
        else if (k == "input_left") in_left = n - 1;
        else if (k == "input_right") in_right = n - 1;
        else if (k == "main_out") main_pair = n > 0 ? (n - 1) / 2 : 0;
        else if (k == "aux1_out" || k == "aux2_out" || k == "aux3_out") aux_pair[k[3] - '1'] = n > 0 ? (n - 1) / 2 : -1;
        else if (k == "midi_in") { midi_in = v; midi_in_set = true; }
        else if (k == "midi_out") midi_out = v;
        else if (k == "show_keyboard") show_keyboard = n != 0;
        else if (k == "display") lcd = (v == "lcd");
        else if (k == "scsi_dir") scsi_dir = v;
        else if (k == "app_dir") app_dir = v;
    }
    fclose(f);
    if (in_left < 0) in_left = 0;
    if (in_right < 0) in_right = 0;
    return true;
}

bool Settings::save() const {
    FILE *f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "# Ensoniq ASR-10 emulator settings\n");
    fprintf(f, "audio_driver=%s\naudio_out=%s\naudio_in=%s\n", audio_api.c_str(), audio_out.c_str(), audio_in.c_str());
    fprintf(f, "sample_rate=%u\nbuffer=%u\n", rate, buffer);
    fprintf(f, "# input channels for the sampler (1 = first input of the device)\ninput_left=%d\ninput_right=%d\n", in_left + 1, in_right + 1);
    fprintf(f, "# first output of each stereo pair (1 = outs 1-2, 3 = outs 3-4 ...); 0 = AUX mixed into main\n");
    fprintf(f, "main_out=%d\n", main_pair * 2 + 1);
    for (int i = 0; i < 3; i++) fprintf(f, "aux%d_out=%d\n", i + 1, aux_pair[i] < 0 ? 0 : aux_pair[i] * 2 + 1);
    fprintf(f, "midi_in=%s\nmidi_out=%s\n", midi_in.c_str(), midi_out.c_str());
    fprintf(f, "# 1 = show the keyboard strip below the panel (F9 / View > Show Keyboard)\nshow_keyboard=%d\n", show_keyboard ? 1 : 0);
    fprintf(f, "# display: vfd (the original) or lcd (purple graphic LCD)\ndisplay=%s\n", lcd ? "lcd" : "vfd");
    fprintf(f, "# folder of the SCSI drive images (HD0.img, CD5.iso ...); empty = \"scsi\" next to the program\nscsi_dir=%s\n", scsi_dir.c_str());
    fprintf(f, "# folder of the program (written by it; the VST3 plug-in looks for the ROMs and disks there)\napp_dir=%s\n", app_dir.c_str());
    fclose(f);
    return true;
}
