#!/usr/bin/env bash
# SPIRV-Tools and SPIRV-Headers at the commits glslang 15.4.0 pins (known_good.json); googletest is not needed.
set -euo pipefail
G="$HOME/ps4/glslang"
cd "$G/dl"
curl -sSfL --max-time 300 -o spirv-tools.tar.gz https://github.com/KhronosGroup/SPIRV-Tools/archive/33e02568181e3312f49a3cf33df470bf96ef293a.tar.gz
curl -sSfL --max-time 300 -o spirv-headers.tar.gz https://github.com/KhronosGroup/SPIRV-Headers/archive/2a611a970fdbc41ac2e3e328802aed9985352dca.tar.gz
ls -l spirv-tools.tar.gz spirv-headers.tar.gz
sha256sum spirv-tools.tar.gz spirv-headers.tar.gz
mkdir -p "$G/src/External/spirv-tools"
tar xzf spirv-tools.tar.gz -C "$G/src/External/spirv-tools" --strip-components=1
mkdir -p "$G/src/External/spirv-tools/external/spirv-headers"
tar xzf spirv-headers.tar.gz -C "$G/src/External/spirv-tools/external/spirv-headers" --strip-components=1
