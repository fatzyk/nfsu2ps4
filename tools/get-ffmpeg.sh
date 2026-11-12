#!/usr/bin/env bash
# FFmpeg tag n7.1 (what nfsu2-sw's tools/build_ffmpeg_vp6.sh uses), into a new empty folder.
set -euo pipefail
F="$HOME/ps4/ffmpeg"
mkdir "$F" "$F/dl"
cd "$F/dl"
curl -sSfL --max-time 600 -o ffmpeg-n7.1.tar.gz https://github.com/FFmpeg/FFmpeg/archive/refs/tags/n7.1.tar.gz
ls -l ffmpeg-n7.1.tar.gz
sha256sum ffmpeg-n7.1.tar.gz
mkdir "$F/src"
tar xzf ffmpeg-n7.1.tar.gz -C "$F/src" --strip-components=1
ls "$F/src" | head -8
grep -m1 -n "FFMPEG_VERSION\|^7" "$F/src/RELEASE" 2>/dev/null || cat "$F/src/RELEASE"
