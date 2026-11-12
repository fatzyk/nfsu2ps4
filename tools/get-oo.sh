#!/usr/bin/env bash
set -euo pipefail
D="$HOME/ps4/oo054"
mkdir "$D"
cd "$D"
curl -sSfL --max-time 900 -o toolchain-llvm-18.tar.gz \
  https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.4/toolchain-llvm-18.tar.gz
ls -l toolchain-llvm-18.tar.gz
sha256sum toolchain-llvm-18.tar.gz
mkdir x
tar xzf toolchain-llvm-18.tar.gz -C x
echo "--- top level"
ls x
echo "--- hits"
find x \( -iname 'right.sprx' -o -iname 'libc.prx' -o -iname 'libSceFios2.prx' -o -iname 'icon0.png' -o -iname '*.prx' -o -iname '*.sprx' \) -exec ls -l {} \;
