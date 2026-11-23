#!/bin/bash
# Builds the ASR-10 emulator for macOS (universal: Apple Silicon and Intel): the "ASR-10.app" app, the
# "ASR10.vst3" plug-in and the "ASR10.component" Audio Unit (Logic Pro), and packages them twice:
#   dist/ASR10-Emulator-macOS-Portable.zip    a folder to unzip anywhere
#   dist/ASR10-Emulator-macOS-Installer.pkg   an installer (with an uninstaller)
#
# Requires:
#   - Apple Command Line Tools (free):          xcode-select --install
#   - CMake 3.25 or newer:                      brew install cmake   (or the .dmg from cmake.org)
#   - an internet connection (the script downloads the VST3 SDK and SDL2 the first time)
#
# Usage: ./build-mac.sh           builds everything and installs the plug-ins in ~/Library/Audio/Plug-Ins
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
if ! ls roms/*.bin >/dev/null 2>&1; then
    echo "Warning: the roms/ folder does not have the ASR-10 ROMs. The app and the plug-in will look for them in"
    echo "  ~/Library/Application Support/Ensoniq/ASR10emu/roms   (the ROM files and os353.img)"
fi

JOBS=$(sysctl -n hw.ncpu)

echo "=== 1/3  app ASR-10.app ==="
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-mac -j"$JOBS" --target asr10 asr10-cli

echo "=== 2/3  plug-ins ASR10.vst3 and ASR10.component ==="
cmake -S plugin -B build-vst -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-vst -j"$JOBS" --target ASR10 ASR10_AU

mkdir -p dist
rm -rf dist/ASR-10.app dist/ASR10.vst3 dist/ASR10.component dist/asr10-cli
cp -R build-mac/ASR-10.app dist/
cp build-mac/asr10-cli dist/
VST=$(find build-vst -name "ASR10.vst3" -type d -maxdepth 4 | grep -v "/Resources/" | head -1)
cp -R "$VST" dist/
cp -R build-vst/AU/ASR10.component dist/

# "ad hoc" signature: required on Apple Silicon to run locally built code (the VST3 inside the
# component is signed first)
codesign --force --deep -s - dist/ASR-10.app
codesign --force --deep -s - dist/ASR10.vst3
codesign --force --deep -s - dist/ASR10.component/Contents/Resources/plugin.vst3
codesign --force -s - dist/ASR10.component
codesign --force -s - dist/asr10-cli

# Portable package, as for Windows: the app, VST3/ASR10.vst3, AU/ASR10.component, empty roms/ and scsi/
# folders (with their READMEs), the READMEs and the licenses. The ROMs, OS disks and disk images are
# never put in it.
PKG=dist/ASR10-macOS
rm -rf "$PKG" dist/ASR10-Emulator-macOS*.zip dist/ASR10-Emulator-macOS*.pkg
mkdir -p "$PKG/VST3" "$PKG/AU" "$PKG/licenses" "$PKG/roms" "$PKG/scsi"
cp -R dist/ASR-10.app "$PKG/"
cp -R dist/ASR10.vst3 "$PKG/VST3/"
cp -R dist/ASR10.component "$PKG/AU/"
cp dist/asr10-cli "$PKG/"
cp roms/README.txt "$PKG/roms/"
cp scsi/README.txt "$PKG/scsi/"
cp README-MAC.txt "$PKG/README.txt"
cp README.md "$PKG/README.md"
cp LICENSES/*.txt musashi/LICENSE-Musashi.txt third_party/RtMidi-LICENSE.txt third_party/rtaudio/RtAudio-LICENSE.txt "$PKG/licenses/"
[ -f build-mac/_deps/sdl2-src/LICENSE.txt ] && cp build-mac/_deps/sdl2-src/LICENSE.txt "$PKG/licenses/SDL2-LICENSE.txt"
VSTSDK_LIC=$(find build-vst/_deps -maxdepth 2 -path "*vst3sdk-src*" -name LICENSE.txt 2>/dev/null | head -1)
[ -n "$VSTSDK_LIC" ] && cp "$VSTSDK_LIC" "$PKG/licenses/VST3-SDK-LICENSE.txt"
ditto -c -k --keepParent "$PKG" "dist/ASR10-Emulator-macOS-Portable.zip"
echo "Portable package: dist/ASR10-Emulator-macOS-Portable.zip"

echo "=== 3/3  installer ==="
installer/mac/make-pkg.sh "$PKG" "dist/ASR10-Emulator-macOS-Installer.pkg"

if [ $INSTALL = 1 ]; then
    mkdir -p ~/Library/Audio/Plug-Ins/VST3
    rm -rf ~/Library/Audio/Plug-Ins/VST3/ASR10.vst3
    cp -R dist/ASR10.vst3 ~/Library/Audio/Plug-Ins/VST3/
    mkdir -p ~/Library/Audio/Plug-Ins/Components
    rm -rf ~/Library/Audio/Plug-Ins/Components/ASR10.component
    cp -R dist/ASR10.component ~/Library/Audio/Plug-Ins/Components/
    killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
    echo "Plug-ins installed in ~/Library/Audio/Plug-Ins/VST3/ASR10.vst3 and ~/Library/Audio/Plug-Ins/Components/ASR10.component"
fi
echo
echo "Done. The app is in dist/ASR-10.app (you can drag it to Applications)."
