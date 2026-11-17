#!/bin/sh
# Build this prototype with fm1-base (base/, linked to Mighty Arp's): the same stack and toolchain.
#   ./build.sh [--release X.Y] [--host | --web]
# The environment as Mighty Arp's build.sh: JIELI_TOOLCHAIN, AC79_SDK, PYTHON, FRIBIDI_LIB, EMSDK.
set -e
cd "$(dirname "$0")"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-python3}"
[ -f base/tools/build.py ] || { echo "build.sh: no fm1-base in base/"; exit 1; }
FRIBIDI_LIB="${FRIBIDI_LIB:-$HOME/.local/fribidi-arm64/lib}"
if [ "$(uname -s)" = Darwin ] && [ -f "$FRIBIDI_LIB/libfribidi.0.dylib" ]; then
    PY="$("$PY" -c 'import sys; print(sys.executable)')"
    export DYLD_FALLBACK_LIBRARY_PATH="$FRIBIDI_LIB${DYLD_FALLBACK_LIBRARY_PATH:+:$DYLD_FALLBACK_LIBRARY_PATH}"
fi
if ! command -v docker >/dev/null 2>&1 && [ -x /Applications/Docker.app/Contents/Resources/bin/docker ]; then
    PATH="$PATH:/Applications/Docker.app/Contents/Resources/bin"
fi
if ! command -v emcc >/dev/null 2>&1 && [ -f "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" ]; then
    . "${EMSDK:-$HOME/emsdk}/emsdk_env.sh" >/dev/null 2>&1
fi
"$PY" base/tools/build.py app "$@"
case " $* " in
*" --web "*) "$PY" tools_arrows.py app/build/emu/index.html   # the arrows and Enter play the cursor
    sed -i.bak 's/MightyArcade/Mighty Arcade/g' app/build/emu/index.html && rm -f app/build/emu/index.html.bak ;;
esac
