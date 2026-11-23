ENSONIQ ASR-10 EMULATOR FOR WINDOWS
===================================

Requires Windows 10 or 11 (64-bit). No DLLs needed.

INSTALLER OR PORTABLE
   ASR10-Emulator-Windows-Setup.exe installs the program in
   C:\Program Files\ASR-10 Emulator (its roms and scsi folders are made
   writable for you), the VST3 plug-in in C:\Program Files\Common Files\VST3
   (optional) and Start menu shortcuts (one opens the roms folder). Uninstall
   with Settings > Apps or the Start menu: it asks whether to keep your ROMs,
   disk images and settings.
   ASR10-Emulator-Windows-Portable.zip is the same program in a folder: unzip
   it anywhere, nothing is installed (delete the folder to remove it).
   Windows may warn that the installer or the program is from an unknown
   publisher (they are not signed): "More info" > "Run anyway".

1. Copy into the "roms" folder (next to asr10.exe):
     asr-648c-lo-1.5b.bin
     asr-65e0-hi-1.5b.bin
     os353.img            (OS floppy disk image)

2. Double-click asr10.exe. If you don't give it another disk, it uses
   roms\os353.img (or, if that is missing, the first .img in the roms folder).

   You see "LOADING SYSTEM" and then the first file on the disk.
   ENTER/YES loads the bank; then press an Instrument/Track button.

WINDOW MENUS
   File: Open Disk Image (choose an .img), Eject Disk (Ctrl+E), Save Disk
         Changes (Ctrl+S), Reset (Ctrl+R), Exit.
   View: Show Keyboard (F9) shows/hides the on-screen keyboard; Options
         (Ctrl+O) opens the audio and MIDI options.
   Messages (disk inserted, ejected...) appear for a few seconds at the bottom;
   the window title shows the disk in the drive, with "*" if it has unsaved
   changes.

AUDIO AND MIDI (View > Options or Ctrl+O)
   Driver (WASAPI, DirectSound, ASIO or SDL), output, sample rate and buffer.
   Outputs: Main and AUX 1/2/3 as pairs of sound card outputs (1-2, 3-4, ...);
   "mixed into Main" is like an ASR-10 without the Output Expander.
   Send an instrument to an AUX in Edit > Amp, parameter OUT=.
   Sampler input: device and L/R channels (with ASIO, the same as the output).
   MIDI In / MIDI Out: MIDI keyboards, or send the sequencer to another synth.
   APPLY saves your choices to %APPDATA%\Ensoniq\ASR10emu\asr10.cfg.
   If there are audio dropouts (underruns), increase the buffer.

DISPLAY
   View > Options > Display, sound and disks > Screen type: the original VFD (cyan) or a purple
   graphic LCD, like the LCD replacement modules. The choice is saved in asr10.cfg
   and the VST3 plug-in follows it.

OTHER FLOPPY DISKS
   Drag an .img file onto the window (same as inserting the disk).
   Single instruments (.efa/.efe): drag one or several onto the window; they
   are put on a temporary disk that goes into the drive. Then LOAD as usual.
   Ctrl+E ejects, Ctrl+S saves the changes to the image.

BOOTING FROM A SCSI DRIVE
   View > Options > SCSI drives > Boot from: "SCSI drive (no floppy at start)".
   The ASR-10 then starts with an empty floppy drive and boots the OS from a
   SCSI drive that has it (e.g. HD0.img in the "scsi" folder). APPLY restarts
   the ASR-10 at once. "Floppy: the OS disk image" goes back to the usual way.
   Command line (this run only):  asr10.exe --boot scsi   or   --boot floppy
   The OS disk image (os353.img) is looked for ONLY in the "roms" folder next to
   the program: a copy left in %APPDATA%\Ensoniq\ASR10emu is not used, so
   deleting os353.img from roms really removes it.

PATCH SELECT
   The two PATCH SELECT buttons on the keyboard strip work as on the ASR-10
   keyboard: click = held, right click = latched on/off (to have both on, **),
   keys [ and ]. On Edit > Instrument PATCH= the display shows OO, *O, O*, **.

INPUT LEVEL POT
   Left of the input LEDs (the real rack has it on the back panel). Drag it
   up/down or turn the mouse wheel over it; double click = 0 dB. Range: off
   to +18 dB. It works before the A/D: the input LEDs and the OS level meter
   follow it, and too much gain clips, as on the hardware.

FAST LOADING AND SAVING
   While a file is loaded or saved (floppy or SCSI) the ASR-10 runs as fast as
   the computer allows, with the sound muted meanwhile: loads, saves and booting
   take a fraction of the time. While the sequencer plays or records it stays
   at real speed (Audio Tracks on disk and songs keep their timing).
   View > Options > Display, sound and disks > Loading / saving: "Fast"
   (default) or "Real speed (as the ASR-10)". The VST3 follows the app's choice
   (in a host the speed-up is smaller: it uses up to ~40% of each audio block's
   time).

D/A CONVERTERS
   View > Options > Display, sound and disks > D/A converters: "TDA1541A (as the
   ASR-10)" (default) models the ASR-10's converters and the analog section
   after them (16-bit steps, each sample held for a whole period, the output
   filter); "Ideal (perfect converter)" only resamples to the sound card's rate.

DI-10 DIGITAL I/O (S/PDIF)
   Ensoniq's digital I/O option. View > Options: "Digital In (DI-10 S/PDIF)"
   picks a pair of inputs of the input device (your interface's S/PDIF input,
   or any other pair) and "Digital Out (DI-10 S/PDIF)" a pair of outputs. The
   board is fitted as soon as either is used (APPLY restarts the ASR-10: its OS
   looks for the board at start).
   - Digital sampling: press Sample, move the cursor left and take REC SRC up
     to DIGITAL (without the board it stops at MAIN-OUT, as on the real unit).
     The OS measures the incoming rate: the samples get 44.1 or 48 kHz. With
     the interface at 44.1 / 48 kHz the samples arrive untouched (16 bits).
     The receiver behaves as the board's CS8412-CP: it locks to 25-55 kHz (a
     32 kHz source is sampled at 32 kHz but labelled 44.1 kHz by the OS, as on
     the real unit); out of lock it gives silence; on a gap it repeats the last
     good sample. An interface at 88.2/96/176.4/192 kHz is taken to 44.1/48.
   - Digital output: the Main mix (not AUX 1-3), 16 bits, when the current
     effect runs at 44.1 kHz; with a 30 kHz effect it is off, as on the real
     unit. With the interface at 44.1 kHz the data is untouched.
   - VST3: follows the app; its stereo input is also the S/PDIF input and the
     extra output bus "S/PDIF Out (DI-10)" carries the digital output.

DISK IMAGES
   Besides raw .img images, Giebler .EDE (800 KB) and .EDA (1600 KB) disk images
   can go in the drive: drop them on the window, or File > Open Disk Image.
   Changes are written back in the same format. In the VST3 plug-in, click the
   drive slot to choose a file (or drop it on the panel, if the host allows).

RECORD A SONG TO WAV
   File > Record Song to WAV (Ctrl+W): choose a folder. The recording waits,
   starts when you press Play on the sequencer and ends when the song ends or
   you press Stop (it goes on until the sound has died away). The 24-bit WAV
   gets the name of the song / sequence shown on the Seq/Song page (open that
   page once so that the name is known). Ctrl+W again cancels or stops.

ROMS NOT FOUND
   If the ROMs are not found, asr10.exe lists the folders it searched and offers
   "Choose folder...": pick the folder with the ROM files; it is remembered
   (the OS disk image is looked for there too, and the VST3 plug-in uses it).

SCSI ACTIVITY LED
   A red LED above the floppy drive blinks while the SCSI bus is in use: slowly
   while reading, fast while writing (loading, saving, recording to disk).

SCSI (as on a ZuluSCSI)
   Put images in the "scsi" folder: HD0.img = disk on ID 0, CD5.iso = CD-ROM
   on ID 5. You can also drag files with those names onto the window. A file
   with another name but larger than 2 MB (e.g. DISK000.iso) is attached as a
   hard disk on the first free ID (dragged or given on the command line).
   Saves go straight to the image file.
   Another folder: View > Options > SCSI drives > Drive images folder >
   Choose folder... (stored in asr10.cfg and also used by the VST3). After
   changing it, hold Power ~1 s so that the OS looks for the new drives.
   New disk: in the same options choose "New drive size" (50 MB to 2 GB) and
   press "New drive  Create HDn.img". It is already formatted (like the
   ASR-10's FORMAT SCSI DRIVE), on the first free SCSI ID in the folder.

COMPUTER KEYBOARD (the PC keyboard does not play notes: use a MIDI keyboard
or the on-screen keyboard)
   1-8 (and F1-F8) = Instrument/Track 1-8
   numeric keypad = page buttons as printed: 1 Env 1, 2 Env 2, 3 Env 3,
     4 Pitch, 5 Filters, 6 Amp, 7 LFO, 8 Wave, 9 Layer, 0 Track
   R = RECORD (hold it and press Space to record),  Space = PLAY
   arrows = Up/Down/Left/Right,  Enter = ENTER/YES,  Esc = CANCEL/NO
   up + down together = the value jumps to the middle of the range (0 in
     -99..+99, 50 in 0..99), as on the hardware. With the mouse: right-click
     one of the arrows.
   Ctrl+F effects on/off,  F11/F12 = Audio Track A/B
   F9 = hide/show the on-screen keyboard (View > Show Keyboard)
   With the mouse: click the buttons and keys (lower = louder), drag the
   sliders and the wheels. Hold Power for ~1 s to restart; the drive button
   ejects the disk. RECORD latches: one click presses it, another releases it.

AUDIO TRACKS (RAMTracks and DiskTracks)
   Button A or B selects the track (yellow LED); pressing it again toggles
   input monitoring (red LED). Switch the input on (Ctrl+I), choose the
   sequence, click RECORD (it latches) and PLAY. At the end, KEEP = OLD NEW:
   ENTER keeps the recording. For DiskTracks: Command > System > CONFIGURE
   AUDIO TRACKS, ATRK PLAY/REC = SCSI, choose the REC SCSI DRIVE and the REC
   FILE size; recording and playback are then done on the SCSI disk.

VST3 PLUG-IN (VST3 folder)
   1. (The installer does this.) Copy the whole VST3\ASR10.vst3 folder to
        C:\Program Files\Common Files\VST3
   2. Open asr10.exe once: it records its folder and the plug-in then uses the
      same files (ROMs and os353.img from the roms folder, scsi folder).
   3. In the host (Cubase, Studio One, Reaper, Bitwig, Live...) rescan
      plug-ins and open "ASR-10 Emulator" as an instrument.
   - Outputs Main + AUX 1/2/3 (enable the AUX outputs in the host to get them
     separately).
   - Stereo input (side chain) for sampling and recording Audio Tracks.
   - MIDI In and MIDI Out.
   - Panel only: drag .img or .efa/.efe files onto the panel; the drive's
     eject button ejects; holding Power ~1 s resets.
   - The plug-in uses the app's settings (for example the SCSI drives folder):
     to change them, open asr10.exe, change them in Options and the plug-in
     follows.
   - The project stores the volume and the disks; when opened, the ASR boots
     again.

OPTIONS (in a command window, e.g. "cmd" in this folder)
   asr10.exe --list-audio                list audio drivers and devices
   asr10.exe --list-midi                 list MIDI ports
   asr10.exe --ram 2                     only 2 MB (default 16 MB, the maximum)
   asr10.exe other.img                   boot with another disk
   asr10-cli.exe --help                  version without a window (tests, WAV recording)

The ASR-10 ROM and operating system are not included.
Component licenses are in the "licenses" folder. ASIO support uses the
Steinberg ASIO SDK (available under GPLv3), so this program is GPLv3.
ASIO and VST are registered trademarks of Steinberg Media Technologies GmbH.
The plug-in uses the VST3 SDK (MIT license), VSTGUI (BSD-3-Clause license)
and SDL2 (zlib license). The panel lettering is rendered from GNU FreeSans
(GPLv3 with the font exception).
