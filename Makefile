CXX ?= g++
CC ?= gcc
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wno-unused-function
CFLAGS ?= -O2
INC = -Imusashi -Isrc

MUSASHI_OBJ = build/m68kcpu.o build/m68kops.o build/m68kdasm.o build/softfloat.o
CORE_OBJ = build/asr10.o build/m68302.o build/duart.o build/fdc.o build/es5506.o build/es5510.o build/panel.o build/scsi.o

all: asr10-cli

build:
	mkdir -p build

musashi/m68kops.c: musashi/m68k_in.c musashi/m68kmake.c
	$(CC) -o build/m68kmake musashi/m68kmake.c && ./build/m68kmake musashi musashi/m68k_in.c

build/m68kcpu.o: musashi/m68kcpu.c musashi/m68kops.c | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/m68kops.o: musashi/m68kops.c | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/m68kdasm.o: musashi/m68kdasm.c | build
	$(CC) $(CFLAGS) $(INC) -c $< -o $@
build/softfloat.o: musashi/softfloat/softfloat.c | build
	$(CC) $(CFLAGS) $(INC) -Imusashi/softfloat -c $< -o $@

build/%.o: src/%.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@

asr10-cli: $(MUSASHI_OBJ) $(CORE_OBJ) build/cli.o
	$(CXX) -o $@ $^

clean:
	rm -rf build asr10-cli asr10

# ---- SDL2 front end (needs SDL2 dev files; RtMidi is built in for MIDI I/O)
SDL_CFLAGS ?= $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   ?= $(shell sdl2-config --libs 2>/dev/null)
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  MIDI_DEF  = -D__MACOSX_CORE__
  MIDI_LIBS = -framework CoreMIDI -framework CoreAudio -framework CoreFoundation
else ifeq ($(OS),Windows_NT)
  MIDI_DEF  = -D__WINDOWS_MM__
  MIDI_LIBS = -lwinmm
else ifneq ($(shell pkg-config --exists alsa && echo yes),)
  MIDI_DEF  = -D__LINUX_ALSA__
  MIDI_LIBS = -lasound -lpthread
else
  $(warning ALSA dev files not found: building without MIDI I/O (install libasound2-dev))
  MIDI_DEF  = -D__RTMIDI_DUMMY__
  MIDI_LIBS = -lpthread
endif

# RtAudio back ends (Linux: ALSA / PulseAudio / JACK if their dev files are installed)
ifeq ($(UNAME_S),Darwin)
  AUDIO_DEF  = -D__MACOSX_CORE__
  AUDIO_LIBS = -framework CoreAudio -framework CoreFoundation
else ifeq ($(OS),Windows_NT)
  AUDIO_DEF  = -D__WINDOWS_WASAPI__ -D__WINDOWS_DS__
  AUDIO_LIBS = -lole32 -lwinmm -lksuser -lmfplat -lmfuuid -ldsound -luuid
else
  AUDIO_DEF  =
  AUDIO_LIBS =
  ifneq ($(shell pkg-config --exists alsa && echo yes),)
    AUDIO_DEF += -D__LINUX_ALSA__
    AUDIO_LIBS += -lasound
  endif
  ifneq ($(shell pkg-config --exists libpulse-simple && echo yes),)
    AUDIO_DEF += -D__LINUX_PULSE__
    AUDIO_LIBS += $(shell pkg-config --libs libpulse-simple)
  endif
  ifneq ($(shell pkg-config --exists jack && echo yes),)
    AUDIO_DEF += -D__UNIX_JACK__
    AUDIO_LIBS += $(shell pkg-config --libs jack)
  endif
  ifeq ($(strip $(AUDIO_DEF)),)
    AUDIO_DEF = -D__RTAUDIO_DUMMY__
  endif
endif

build/rtmidi.o: third_party/RtMidi.cpp | build
	$(CXX) $(CXXFLAGS) $(MIDI_DEF) -c $< -o $@
build/sdl_main.o: src/sdl_main.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(INC) $(SDL_CFLAGS) -DASR_WITH_RTMIDI -c $< -o $@
build/vfd.o: src/vfd.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(INC) $(SDL_CFLAGS) -c $< -o $@
build/native_menu.o: src/native_menu.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(INC) $(SDL_CFLAGS) -c $< -o $@
build/ui_text.o: src/ui_text.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(INC) $(SDL_CFLAGS) -c $< -o $@
build/rack_ui.o: src/rack_ui.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(INC) $(SDL_CFLAGS) -c $< -o $@
build/audio_io.o: src/audio_io.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(INC) $(SDL_CFLAGS) -DASR_WITH_RTAUDIO -c $< -o $@
build/settings.o: src/settings.cpp src/*.h | build
	$(CXX) $(CXXFLAGS) $(INC) -c $< -o $@
build/rtaudio.o: third_party/rtaudio/RtAudio.cpp | build
	$(CXX) $(CXXFLAGS) $(AUDIO_DEF) -Ithird_party/rtaudio/include -c $< -o $@
asr10: $(MUSASHI_OBJ) $(CORE_OBJ) build/sdl_main.o build/vfd.o build/rack_ui.o build/ui_text.o build/native_menu.o build/audio_io.o build/settings.o build/rtmidi.o build/rtaudio.o
	$(CXX) -o $@ $^ $(SDL_LIBS) $(MIDI_LIBS) $(AUDIO_LIBS) -lpthread
