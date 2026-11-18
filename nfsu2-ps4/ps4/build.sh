#!/usr/bin/env bash
# Build the NFSU2 (Xbox) recompilation for the PlayStation 4 against the pinned orbis-sdk-v1 bundle.
#
#   ps4/build.sh            configure + build  ->  $BUILD/eboot.bin
#   ps4/build.sh pkg        ... and package    ->  $OUT/IV0000-NFSU20004_00-NFSU200040000000.pkg
#
# Environment (defaults match the WSL layout in PS4_NOTES.md):
#   SDK            orbis-sdk-v1 bundle                 $HOME/ps4/sdk/orbis-sdk-v1
#   OO             OpenOrbis v0.5.4 tree (PRX, sprx)   $HOME/ps4/oo054/x/OpenOrbis/PS4Toolchain
#   NFSU2_GEN_DIR  lifted C from tools/regen.sh        $HOME/nfsu2x/gen
#   BUILD          build directory                     $HOME/ps4/build-nfsu2
#   OUT            package output                      $HOME/ps4/out-nfsu2
#   JOBS           parallel jobs (default: nproc, memory-capped)
#   OPENSSL_LIB    dir with libssl.so.1.1 for PkgTool  $HOME/ps4/openssl11/lib
#   GLSLANG        glslang install (ps4/build-glslang)  $HOME/ps4/glslang/install
#   PKG_ICON       optional local 512x512 PNG          U2 community-port icon
#   PKG_VERSION    package version                    01.16
#   VULKAN         1 = Vulkan renderer (default), 0 = CPU executor only
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="${SDK:-$HOME/ps4/sdk/orbis-sdk-v1}"
OO="${OO:-$HOME/ps4/oo054/x/OpenOrbis/PS4Toolchain}"
GEN="${NFSU2_GEN_DIR:-$HOME/nfsu2x/gen}"
BUILD="${BUILD:-$HOME/ps4/build-nfsu2}"
OUT="${OUT:-$HOME/ps4/out-nfsu2}"
OPENSSL_LIB="${OPENSSL_LIB:-$HOME/ps4/openssl11/lib}"
GLSLANG="${GLSLANG:-$HOME/ps4/glslang/install}"
# VULKAN=1 (default): the Vulkan renderer (nv2a_vk, glslang for the PS4); VULKAN=0: the CPU executor only (M2 build)
#   FFMPEG_DIR     VP6-only FFmpeg (ps4/build_ffmpeg_vp6.sh)  $HOME/ps4/ffmpeg-vp6-ps4; empty = lifted VP6 decoder
FFMPEG_DIR="${FFMPEG_DIR-$HOME/ps4/ffmpeg-vp6-ps4}"
[ -f "$FFMPEG_DIR/lib/libavcodec.a" ] || FFMPEG_DIR=""
if [ "${VULKAN:-1}" = "1" ]; then VK_FLAG=ON; else VK_FLAG=OFF; fi

[ -f "$SDK/toolchain/orbis-sdk.cmake" ] || { echo "error: no orbis SDK at $SDK" >&2; exit 1; }
[ -f "$GEN/recomp_funcs.h" ] || { echo "error: no generated code in $GEN (run tools/regen.sh)" >&2; exit 1; }

if [ -z "${JOBS:-}" ]; then
    mem_mb=$(( $(awk '/^MemAvailable:/ {print $2}' /proc/meminfo) / 1024 ))
    JOBS=$(( mem_mb / 1500 )); [ "$JOBS" -lt 1 ] && JOBS=1
    [ "$JOBS" -gt "$(nproc)" ] && JOBS=$(nproc)
fi

cmake -S "$REPO" -B "$BUILD" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$SDK/toolchain/orbis-sdk.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DNFSU2_GEN_OPT=-O2 \
    -DNFSU2_GEN_DIR="$GEN" -DNFSU2_VULKAN="$VK_FLAG" -Dglslang_DIR="$GLSLANG/lib/cmake/glslang" \
    -DSPIRV-Tools-opt_DIR="$GLSLANG/lib/cmake/SPIRV-Tools-opt" -DSPIRV-Tools_DIR="$GLSLANG/lib/cmake/SPIRV-Tools" \
    -DNFSU2_FFMPEG_DIR="$FFMPEG_DIR" \
    > "$BUILD.configure.log" 2>&1 || { tail -30 "$BUILD.configure.log"; exit 1; }
ninja -C "$BUILD" -j"$JOBS"
echo "built $BUILD/eboot.bin ($(stat -c %s "$BUILD/eboot.bin") bytes)"

if [ "${1:-}" = "pkg" ]; then
    export PS4_PKGTOOL_OPENSSL_LIB="$OPENSSL_LIB"
    ICON="${PKG_ICON:-$REPO/ps4/art/icon0.png}"
    mkdir -p "$OUT"
    bash "$SDK/orbis-compat/scripts/ps4/make-pkg.sh" \
        --eboot "$BUILD/eboot.bin" --out-dir "$OUT" \
        --title-id NFSU20004 --title "Need for Speed: Underground 2" --version "${PKG_VERSION:-01.16}" \
        --sdk "$OO" \
        --icon "$ICON" \
        --extra "$OO/src/modules/right.sprx:sce_sys/about/right.sprx"
fi
