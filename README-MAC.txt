ENSONIQ ASR-10 EMULATOR FOR macOS (Apple Silicon) - APP AND VST3 PLUG-IN
========================================================================

This package contains the source code; build it once on your Mac.

1. Prepare (once)
   - Install Apple's Command Line Tools: open Terminal and type
         xcode-select --install
   - Install CMake: with Homebrew,  brew install cmake
     (or download the .dmg from https://cmake.org/download/ and copy CMake to Applications).

2. ROMs and OS disk
   Copy the ASR-10 ROMs and the OS floppy disk image (os353.img) into the "roms" folder
   (next to this file). They are bundled into the app and the plug-in.
   Alternatively, put them in  ~/Library/Application Support/Ensoniq/ASR10emu/roms

3. Build
   In Terminal, inside this folder:
         ./build-mac.sh
   It takes a few minutes the first time (it downloads the VST3 SDK and SDL2).
   When it finishes:
     dist/ASR-10.app   -> drag it to Applications
     dist/asr10-cli    -> command line tool
     the plug-in is installed in ~/Library/Audio/Plug-Ins/VST3/ASR10.vst3

4. The app
   - The first time you use the audio input, macOS asks for microphone permission.
   - Floppy disks and SCSI disks are dragged onto the window; single instruments (.efa/.efe)
     too (they go on a temporary floppy). The OS disk is copied to
     ~/Library/Application Support/Ensoniq/ASR10emu and that copy is the one used/saved.
   - SCSI disks: by default the "scsi" folder inside that same folder (names HD0.img,
     CD5.iso, ...), or any image larger than 2 MB dragged onto the window. Another folder and
     new formatted disks (50 MB to 2 GB): View > Options > SCSI drives.

5. The VST3 plug-in (instrument)
   - It appears as "ASR-10 Emulator" in any VST3 host (Cubase, Studio One, Reaper, Bitwig,
     Ableton Live 11+...). Logic does not use VST3.
   - Outputs: Main + AUX 1/2/3. Enable the AUX outputs in the host to get them separately.
   - Stereo input (side chain) for sampling / recording Audio Tracks.
   - MIDI In and MIDI Out (the ASR's sequencer can play other instruments).
   - Panel only: drag disk images or .efa/.efe files onto the panel; the drive's eject button
     ejects; holding Power ~1 s resets.
   - The plug-in uses the app's settings and files (ROMs, SCSI folder...): to change them,
     open the app, change them in Options and the plug-in follows.
   - The project stores the volume and the attached disks; when opened, the ASR boots again
     with them (sounds load as on a real ASR).

If macOS says the app "cannot be opened": right-click it > Open.

MENUS (Mac menu bar)
   File: Open Disk Image (Cmd+O), Eject Disk (Cmd+E), Save Disk Changes (Cmd+S),
         Reset (Cmd+R).
   View: Show Keyboard (Cmd+K) and Options (Cmd+,).

The ASR-10 ROM and operating system are not included.
