#!/usr/bin/env bash
set -euo pipefail

die() { printf '%s\n' "$*" >&2; exit 1; }
[[ $# == 1 ]] || die 'Usage: bash tools/build-ps4-package.sh /path/to/extracted/game'
game="$(cd -- "$1" && pwd)" || die 'Game folder not found.'
[[ -f "$game/default.xbe" && -d "$game/NFSUNDER" ]] ||
    die 'Choose the extracted Xbox game folder containing default.xbe and NFSUNDER.'
hash="$(sha256sum "$game/default.xbe")"
[[ ${hash%% *} == 95c22b9c594f0c187f602ac6484fe4cb65a9d53dafe3110ed815598eb9dcb0bd ]] ||
    die 'This build requires the supported original Xbox NTSC-U default.xbe.'

root="$(cd -- "$(dirname -- "$0")/.." && pwd)"
export PATH="$HOME/ps4/bin:$PATH"
if ! (for cmd in curl clang ld.lld llvm-ar cmake ninja make nasm; do
          command -v "$cmd" >/dev/null || exit 1
      done
      python3 -c 'import capstone, pefile, numpy'); then
    sudo apt-get update
    sudo apt-get install -y build-essential curl clang lld llvm cmake ninja-build nasm \
        python3 python3-capstone python3-pefile python3-numpy
fi

p="$HOME/ps4"
mkdir -p "$p" "$root/out"
work="$(mktemp -d "$p/package-build.XXXXXX")"
out="$(mktemp -d "$root/out/ps4.XXXXXX")"
trap 'printf "Build failed. Working files: %s\n" "$work" >&2' ERR
fetch() {
    curl -fsSL --retry 3 "$1" -o "$work/download"
    printf '%s  %s\n' "$2" "$work/download" | sha256sum -c -
}
export SDK="$p/sdk/orbis-sdk-v1" OO="$p/oo054/x/OpenOrbis/PS4Toolchain"
if [[ ! -f "$SDK/toolchain/orbis-sdk.cmake" ]]; then
    fetch https://github.com/orbis-ports/orbis-porting-kit/releases/download/orbis-sdk-v1/orbis-sdk-v1.tar.gz \
        f4f079a4df334b46a76996f895283904283d6a006f97102aafaa53d4ecfc9622
    mkdir -p "$p/sdk"
    tar xzf "$work/download" -C "$p/sdk"
fi
if [[ ! -f "$OO/src/modules/right.sprx" ]]; then
    fetch https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.4/toolchain-llvm-18.tar.gz \
        3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526
    mkdir -p "$p/oo054/x"
    tar xzf "$work/download" -C "$p/oo054/x"
fi
export OPENSSL_LIB="$p/openssl11/lib"
if [[ ! -f "$OPENSSL_LIB/libssl.so.1.1" ]]; then
    fetch https://security.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1-1ubuntu2.1~18.04.23_amd64.deb \
        986727f5a99088bef98971edfcd9567f5693b5e941aa241894fce1020c7015a0
    dpkg-deb -x "$work/download" "$work/openssl"
    mkdir -p "$OPENSSL_LIB"
    cp "$work/openssl/usr/lib/x86_64-linux-gnu/"lib{ssl,crypto}.so.1.1 "$OPENSSL_LIB/"
fi
if [[ ! -f "$p/glslang/install/lib/cmake/glslang/glslang-config.cmake" ]]; then
    [[ -d "$p/glslang/src" ]] || bash "$root/tools/get-glslang.sh"
    [[ -f "$p/glslang/src/External/spirv-tools/external/spirv-headers/CMakeLists.txt" ]] ||
        bash "$root/tools/get-spirv.sh"
    JOBS=4 bash "$root/tools/build-glslang.sh"
fi
export FFMPEG_DIR="$p/ffmpeg-vp6-ps4"
if [[ ! -f "$FFMPEG_DIR/lib/libavcodec.a" ]]; then
    [[ -f "$p/ffmpeg/src/configure" ]] || bash "$root/tools/get-ffmpeg.sh"
    bash "$root/nfsu2-sw/ps4/build_ffmpeg_vp6.sh"
fi

cp -a "$root/nfsu2-sw" "$work/source"
cp "$game/default.xbe" "$work/default.xbe"
export NFSU2_XBE="$work/default.xbe" NFSU2_GEN_DIR="$work/gen"
cd "$work/source/xboxrecomp"
python3 -m tools.xbe_parser "$NFSU2_XBE" --json "$work/default_analysis.json"
cd "$work/source"
bash tools/regen.sh
GLSLANG="$p/glslang/install" BUILD="$work/build" OUT="$out" VULKAN=1 bash ps4/build.sh pkg
pkg="$out/IV0000-NFSU20004_00-NFSU200040000000.pkg"
[[ -s "$pkg" ]] || die "Package missing: $pkg"
printf '\nPackage ready: %s\n' "$pkg"
