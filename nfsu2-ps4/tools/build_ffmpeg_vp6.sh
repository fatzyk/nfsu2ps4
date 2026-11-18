#!/usr/bin/env bash
# Build the minimal FFmpeg the movie decoder links (src/movie_vp6.c):
# libavcodec + libavutil with only the vp6 decoder. LGPL 2.1 -- no GPL parts
#
#   tools/build_ffmpeg_vp6.sh linux
#
# Environment:
#   FFMPEG_SRC   FFmpeg source (default /root/nfsu2x/ref/ffmpeg-7.1,
#                git clone -b release/7.1 https://github.com/FFmpeg/FFmpeg.git)
#   PREFIX       install prefix (default /root/nfsu2x/ffmpeg-vp6-linux)
set -euo pipefail

[[ $# -eq 0 || ( $# -eq 1 && $1 == linux ) ]] || { echo "usage: $0 linux" >&2; exit 1; }
SRC="${FFMPEG_SRC:-/root/nfsu2x/ref/ffmpeg-7.1}"
PREFIX="${PREFIX:-/root/nfsu2x/ffmpeg-vp6-linux}"
BUILD="$PREFIX-build"

COMMON=(--prefix="$PREFIX" --disable-everything --enable-decoder=vp6
        --disable-avformat --disable-avdevice --disable-avfilter --disable-swscale
        --disable-swresample --disable-programs --disable-doc --disable-network
        --disable-pthreads --disable-debug --disable-autodetect
        --enable-static --disable-shared --enable-pic)

mkdir -p "$BUILD"
cd "$BUILD"
"$SRC/configure" "${COMMON[@]}" --disable-x86asm >configure.log
grep -E '^License' configure.log
make -j"$(nproc)" >make.log
make install >install.log
echo "installed $PREFIX (pass -DNFSU2_FFMPEG_DIR=$PREFIX)"
