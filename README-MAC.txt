ENSONIQ ASR-10 EMULATOR FOR macOS (Apple Silicon and Intel) - APP, VST3 AND AUDIO UNIT
=====================================================================================

This file covers the ready-made packages and building from the source code. The app and the plug-ins are
universal: they run on Apple Silicon and on Intel Macs (macOS 11 or later). Get the packages from the
release page, or from GitHub Actions (https://github.com/dudumil/Ensoniq-ASR-10-Emulator/actions >
"macOS" workflow > latest green run > "Artifacts" at the bottom).

OPTION A1 - installer (ASR10-Emulator-macOS-Installer.pkg)
   1. Open the .pkg. It is not signed by Apple: if macOS refuses to open it, right-click it > Open
      (or System Settings > Privacy & Security > "Open Anyway").
   2. It installs (each part can be left out with "Customize"):
         Applications > ASR-10 Emulator        the app, roms/ and scsi/ (writable), READMEs and
                                               "Uninstall ASR-10 Emulator"
         /Library/Audio/Plug-Ins/VST3/ASR10.vst3            the VST3 plug-in
         /Library/Audio/Plug-Ins/Components/ASR10.component the Audio Unit (Logic Pro, GarageBand)
   3. Put the ROM files (asr-648c-lo-1.5b.bin and asr-65e0-hi-1.5b.bin, or asr_lo.bin / asr_hi.bin) and os353.img in
      Applications > ASR-10 Emulator > roms, then open the app once (the plug-ins then use the same
      ROMs, OS disk and scsi folder).
   4. To remove it, open "Uninstall ASR-10 Emulator" in that folder: it can keep your ROMs, disk images
      and settings.
   Skip to section 4 below.

OPTION A2 - portable package (ASR10-Emulator-macOS-Portable.zip, nothing installed)
   1. Unzip it. The folder ASR10-macOS contains:
         ASR-10.app             the app
         roms/                  put the ROMs and the OS disk image here
         scsi/                  SCSI disk / CD-ROM images (ZuluSCSI names: HD0.img, CD5.iso, ...)
         VST3/ASR10.vst3        the VST3 plug-in
         AU/ASR10.component     the Audio Unit (Logic Pro, GarageBand)
         README.txt, README.md, licenses/
      Keep the app, roms/ and scsi/ together in this folder (it can go anywhere, e.g. in Applications
      or in Documents): the app looks for roms/ and scsi/ next to itself.
   2. Remove the "downloaded from the internet" flag once (Terminal; drag the folder onto the window
      after typing the command and a space to fill in the path):
         xattr -dr com.apple.quarantine ASR10-macOS
      Without it macOS refuses to open the app ("cannot verify ..."), or runs it from a hidden copy
      where roms/ and scsi/ are not seen. (Instead you can open it once and choose
      System Settings > Privacy & Security > "Open Anyway", but xattr also covers the plug-ins.)
   3. Put the ROM files (asr-648c-lo-1.5b.bin and asr-65e0-hi-1.5b.bin, or asr_lo.bin / asr_hi.bin) and os353.img in roms/, then open the app.
      If the ROMs are still not found, the app lists the folders it searched and offers
      "Choose folder...": pick the folder with the ROMs; it is remembered.
   4. Plug-ins: copy VST3/ASR10.vst3 to ~/Library/Audio/Plug-Ins/VST3/ and AU/ASR10.component to
      ~/Library/Audio/Plug-Ins/Components/ (Finder: Go > Go to Folder...).
      Open the app once first: the plug-ins then use the same ROMs, OS disk and scsi folder.
   Skip to section 4 below.

OPTION B - build it yourself on your Mac (sections 1 to 3).
   If "xcode-select --install" says the software is not available on the server, install the
   tools another way: download "Command Line Tools for Xcode" from
   https://developer.apple.com/download/all/ (free Apple ID), or install Xcode from the App Store;
   then, if needed,  sudo xcode-select --reset

1. Prepare (once)
   - Install Apple's Command Line Tools: open Terminal and type
         xcode-select --install
   - Install CMake: with Homebrew,  brew install cmake
     (or download the .dmg from https://cmake.org/download/ and copy CMake to Applications).

2. ROMs and OS disk
   Copy the ASR-10 ROMs and the OS floppy disk image (os353.img) into the "roms" folder
   (next to this file). They are bundled into the app and the plug-in.
   (The ROMs can alternatively go in ~/Library/Application Support/Ensoniq/ASR10emu/roms; the OS disk
   image is only taken from the "roms" folder, never from there.)

3. Build
   In Terminal, inside this folder:
         ./build-mac.sh
   It takes a few minutes the first time (it downloads the VST3 SDK and SDL2).
   When it finishes:
     dist/ASR-10.app   -> drag it to Applications
     dist/asr10-cli    -> command line tool
     dist/ASR10-Emulator-macOS-Portable.zip  -> the package of option A2
     dist/ASR10-Emulator-macOS-Installer.pkg -> the installer of option A1
     the plug-ins are installed in ~/Library/Audio/Plug-Ins/VST3/ASR10.vst3 and
     ~/Library/Audio/Plug-Ins/Components/ASR10.component

4. The app
   - The first time you use the audio input, macOS asks for microphone permission.
   - Floppy disks and SCSI disks are dragged onto the window; single instruments (.efa/.efe)
     too (they go on a temporary floppy). The OS disk is copied to
     ~/Library/Application Support/Ensoniq/ASR10emu and that copy is the one used/saved.
   - Display: View > Options > Display > Screen type chooses the original VFD or a purple graphic LCD.
   - SCSI disks: by default the "scsi" folder inside that same folder (names HD0.img,
     CD5.iso, ...), or any image larger than 2 MB dragged onto the window. Another folder and
     new formatted disks (50 MB to 2 GB): View > Options > SCSI drives.

   Boot from SCSI: Options > SCSI drives > Boot from > "SCSI drive (no floppy at start)" starts with an empty
   floppy drive and boots from a SCSI drive that has the OS (or: open the app with --boot scsi). A red LED
   above the floppy drive blinks on SCSI activity (slowly while reading, fast while writing).

   DI-10 Digital I/O (S/PDIF): Options > Digital In / Digital Out (DI-10 S/PDIF) pick a pair of inputs
   / outputs of the interface; the board is fitted when either is used. Sample > REC SRC=DIGITAL samples
   from it at 44.1 or 48 kHz; the digital output carries the Main mix when the effect runs at 44.1 kHz.

   Fast loading and saving (Options > Display, sound and disks > Loading / saving): while a file loads or saves
   the ASR-10 runs at full speed with the sound muted (not while the sequencer plays: Audio Tracks keep their
   timing). "Real speed" turns it off.

   D/A converters (Options > Display, sound and disks): TDA1541A as the ASR-10 (default: the converters and the
   analog section after them) or Ideal (a perfect converter).

5. The VST3 plug-in and the Audio Unit (instrument)
   - The VST3 appears as "ASR-10 Emulator" in any VST3 host (Cubase, Studio One, Reaper, Bitwig,
     Ableton Live 11+...). Logic Pro and GarageBand use the Audio Unit: Instrument > AU Instruments >
     ASR10emu > ASR-10 Emulator. Both work the same way (the Audio Unit wraps the VST3).
   - Outputs: Main + AUX 1/2/3 (+ S/PDIF Out with the DI-10). Enable them in the host to get them separately.
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
