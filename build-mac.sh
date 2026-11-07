#!/bin/bash
# Compila o emulador ASR-10 para macOS (Apple Silicon): a app "ASR-10.app" e o plug-in "ASR10.vst3".
#
# Precisa de:
#   - Command Line Tools da Apple (gratuitas):  xcode-select --install
#   - CMake 3.25 ou mais recente:               brew install cmake   (ou o .dmg de cmake.org)
#   - ligacao a internet (o script descarrega o SDK VST3 e o SDL2 na primeira vez)
#
# Uso:  ./build-mac.sh            compila tudo e instala o plug-in em ~/Library/Audio/Plug-Ins/VST3
#       ./build-mac.sh --no-install
set -e
cd "$(dirname "$0")"

INSTALL=1
[ "$1" = "--no-install" ] && INSTALL=0

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Faltam as Command Line Tools da Apple. Vou abrir o instalador;"
    echo "quando terminar, corre este script outra vez."
    xcode-select --install || true
    exit 1
fi
if ! command -v cmake >/dev/null 2>&1; then
    if [ -x /Applications/CMake.app/Contents/bin/cmake ]; then
        export PATH="/Applications/CMake.app/Contents/bin:$PATH"
    else
        echo "Falta o CMake. Instala com 'brew install cmake' ou a partir de https://cmake.org/download/"
        exit 1
    fi
fi
if [ "$(uname -m)" != "arm64" ]; then
    echo "Aviso: este Mac nao e Apple Silicon; vou compilar na mesma para arm64."
fi
if ! ls roms/*.bin >/dev/null 2>&1; then
    echo "Aviso: a pasta roms/ nao tem as ROMs do ASR-10. A app e o plug-in vao procura-las em"
    echo "  ~/Library/Application Support/Ensoniq/ASR10emu/roms   (asr_lo.bin, asr_hi.bin e os353.img)"
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

# assinatura "ad hoc": obrigatoria em Apple Silicon para correr codigo compilado localmente
codesign --force --deep -s - dist/ASR-10.app
codesign --force --deep -s - dist/ASR10.vst3
codesign --force -s - dist/asr10-cli

if [ $INSTALL = 1 ]; then
    mkdir -p ~/Library/Audio/Plug-Ins/VST3
    rm -rf ~/Library/Audio/Plug-Ins/VST3/ASR10.vst3
    cp -R dist/ASR10.vst3 ~/Library/Audio/Plug-Ins/VST3/
    echo "Plug-in instalado em ~/Library/Audio/Plug-Ins/VST3/ASR10.vst3"
fi
echo
echo "Pronto. A app esta em dist/ASR-10.app (podes arrasta-la para Aplicacoes)."
