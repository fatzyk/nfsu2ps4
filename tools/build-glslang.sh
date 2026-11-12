#!/usr/bin/env bash
# glslang 15.4.0 (+ pinned SPIRV-Tools with the optimizer) as static libraries for the PS4.
set -euo pipefail
P="$HOME/ps4"
SDK="$P/sdk/orbis-sdk-v1"
G="$P/glslang"
B="$G/build"
I="$G/install"
mkdir -p "$B"
if [ ! -f "$B/build.ninja" ] || [ "${RECONF:-0}" = "1" ]; then
  rm -rf "$B"; mkdir -p "$B"
  cmake -S "$G/src" -B "$B" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$SDK/toolchain/orbis-sdk.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$I" \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_EXTERNAL=ON \
    -DENABLE_GLSLANG_BINARIES=OFF -DENABLE_HLSL=OFF -DENABLE_SPVREMAPPER=OFF -DENABLE_OPT=ON \
    -DENABLE_PCH=OFF -DGLSLANG_TESTS=OFF -DENABLE_CTEST=OFF -DBUILD_TESTING=OFF \
    -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DSPIRV_WERROR=OFF -DSPIRV_BUILD_COMPRESSION=OFF \
    > "$G/configure.log" 2>&1 || { echo CONFIGURE FAILED; tail -40 "$G/configure.log"; exit 1; }
  echo "configure OK"
fi
ninja -C "$B" -j"${JOBS:-10}" > "$G/build.log" 2>&1 || { echo BUILD FAILED; grep -E 'error|Error' "$G/build.log" | head -30; tail -15 "$G/build.log"; exit 1; }
echo BUILD OK
tail -3 "$G/build.log"
cmake --install "$B" > "$G/install.log" 2>&1 || { echo INSTALL FAILED; tail -10 "$G/install.log"; exit 1; }
find "$I" -name '*.a' | sort
