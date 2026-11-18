#!/usr/bin/env bash
# VP6-only, LGPL FFmpeg (libavcodec + libavutil) for the PS4, the counterpart of tools/build_ffmpeg_vp6.sh.
#
#   ps4/build_ffmpeg_vp6.sh          -> $PREFIX (default $HOME/ps4/ffmpeg-vp6-ps4); pass -DNFSU2_FFMPEG_DIR=$PREFIX
#
# Environment: SDK (orbis-sdk-v1), OO (OpenOrbis tree), FFMPEG_SRC (FFmpeg n7.1 source), PREFIX.
# FFmpeg's configure wants to link test programs; the PS4 link needs crt1/libkernel, so the test links ignore unresolved
# symbols (every libc function then "exists", which is fine: musl has what libavcodec/libavutil use).
set -euo pipefail

SDK="${SDK:-$HOME/ps4/sdk/orbis-sdk-v1}"
OO="${OO:-$HOME/ps4/oo054/x/OpenOrbis/PS4Toolchain}"
SRC="${FFMPEG_SRC:-$HOME/ps4/ffmpeg/src}"
PREFIX="${PREFIX:-$HOME/ps4/ffmpeg-vp6-ps4}"
BUILD="$PREFIX-build"
COMPAT="$SDK/orbis-compat"

mkdir -p "$BUILD"
CC_WRAP="$BUILD/ps4-cc.sh"
cat > "$CC_WRAP" <<EOF
#!/usr/bin/env bash
# clang for the PS4 target (flags of orbis-compat's toolchain file); test links ignore unresolved symbols
exec clang --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -D__PS4__ -DPS4 -D__ORBIS__ -D_BSD_SOURCE=1 \\
    -isysroot $SDK/sdk -isystem $COMPAT/include -isystem $SDK/sdk/include "\$@"
EOF
chmod +x "$CC_WRAP"

cd "$BUILD"
"$SRC/configure" --prefix="$PREFIX" \
    --disable-everything --enable-decoder=vp6 \
    --disable-avformat --disable-avdevice --disable-avfilter --disable-swscale --disable-swresample \
    --disable-programs --disable-doc --disable-network --disable-pthreads --disable-debug --disable-autodetect \
    --enable-static --disable-shared --enable-pic \
    --enable-cross-compile --target-os=freebsd --arch=x86_64 --cc="$CC_WRAP" --ar=llvm-ar --ranlib=llvm-ranlib \
    --nm=llvm-nm --strip=llvm-strip --extra-cflags="-O2" \
    --extra-ldflags="-nostdlib -fuse-ld=lld -Wl,--unresolved-symbols=ignore-all -Wl,-shared" \
    > configure.log 2>&1 || { echo CONFIGURE FAILED; tail -40 configure.log; exit 1; }
grep -E '^(License|ARCH|target OS|x86asm|NASM|yasm)' configure.log || true
make -j"$(nproc)" > make.log 2>&1 || { echo MAKE FAILED; grep -E 'error' make.log | head -20; tail -15 make.log; exit 1; }
make install > install.log 2>&1
ls -l "$PREFIX/lib"
echo "installed $PREFIX (pass -DNFSU2_FFMPEG_DIR=$PREFIX)"
