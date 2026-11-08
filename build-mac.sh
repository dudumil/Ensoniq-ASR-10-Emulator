#!/bin/bash
# Builds the ASR-10 emulator for macOS (Apple Silicon): the "ASR-10.app" app and the "ASR10.vst3" plug-in.
#
# Requires:
#   - Apple Command Line Tools (free):          xcode-select --install
#   - CMake 3.25 or newer:                      brew install cmake   (or the .dmg from cmake.org)
#   - an internet connection (the script downloads the VST3 SDK and SDL2 the first time)
#
# Usage: ./build-mac.sh           builds everything and installs the plug-in in ~/Library/Audio/Plug-Ins/VST3
#        ./build-mac.sh --no-install
set -e
cd "$(dirname "$0")"

INSTALL=1
[ "$1" = "--no-install" ] && INSTALL=0

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Apple's Command Line Tools are missing. Opening the installer;"
    echo "when it finishes, run this script again."
    xcode-select --install || true
    exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
    if [ -x /Applications/CMake.app/Contents/bin/cmake ]; then
        export PATH="/Applications/CMake.app/Contents/bin:$PATH"
    else
        echo "CMake is missing. Install it with 'brew install cmake' or from https://cmake.org/download/"
        exit 1
    fi
fi
if [ "$(uname -m)" != "arm64" ]; then
    echo "Warning: this Mac is not Apple Silicon; building for arm64 anyway."
fi
if ! ls roms/*.bin >/dev/null 2>&1; then
    echo "Warning: the roms/ folder does not have the ASR-10 ROMs. The app and the plug-in will look for them in"
    echo "  ~/Library/Application Support/Ensoniq/ASR10emu/roms   (the ROM files and os353.img)"
fi

JOBS=$(sysctl -n hw.ncpu)

echo "=== 1/2  app ASR-10.app ==="
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-mac -j"$JOBS" --target asr10 asr10-cli

echo "=== 2/2  plug-in ASR10.vst3 ==="
cmake -S plugin -B build-vst -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-vst -j"$JOBS" --target ASR10

mkdir -p dist
rm -rf dist/ASR-10.app dist/ASR10.vst3 dist/asr10-cli
cp -R build-mac/ASR-10.app dist/
cp build-mac/asr10-cli dist/
VST=$(find build-vst -name "ASR10.vst3" -type d -maxdepth 4 | head -1)
cp -R "$VST" dist/

# "ad hoc" signature: required on Apple Silicon to run locally built code
codesign --force --deep -s - dist/ASR-10.app
codesign --force --deep -s - dist/ASR10.vst3
codesign --force -s - dist/asr10-cli

if [ $INSTALL = 1 ]; then
    mkdir -p ~/Library/Audio/Plug-Ins/VST3
    rm -rf ~/Library/Audio/Plug-Ins/VST3/ASR10.vst3
    cp -R dist/ASR10.vst3 ~/Library/Audio/Plug-Ins/VST3/
    echo "Plug-in installed in ~/Library/Audio/Plug-Ins/VST3/ASR10.vst3"
fi
echo
echo "Done. The app is in dist/ASR-10.app (you can drag it to Applications)."
