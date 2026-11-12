#!/usr/bin/env bash
# Fetch glslang 15.4.0 and the SPIRV-Tools / SPIRV-Headers revisions its known_good.json pins.
set -euo pipefail
G="$HOME/ps4/glslang"
mkdir "$G" "$G/dl"
cd "$G/dl"
curl -sSfL --max-time 300 -o glslang-15.4.0.tar.gz https://github.com/KhronosGroup/glslang/archive/refs/tags/15.4.0.tar.gz
ls -l glslang-15.4.0.tar.gz
sha256sum glslang-15.4.0.tar.gz
mkdir "$G/src"
tar xzf glslang-15.4.0.tar.gz -C "$G/src" --strip-components=1
echo "--- known_good.json"
cat "$G/src/known_good.json"
