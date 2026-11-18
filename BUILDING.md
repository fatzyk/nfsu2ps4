# Optional: build the PS4 port locally

If you already have the FPKG, skip this guide and follow the [installation steps](README.md#download-and-install). To build your own package, use an extracted **original Xbox NTSC-U (USA)** copy of the game.

> Just throw it to Claude and tell him to make no mistakes

## One-command build

On Ubuntu 24.04 (including WSL2), run this from the repository root:

```bash
bash tools/build-ps4-package.sh /path/to/extracted/game
```

Use the folder containing `default.xbe` and `NFSUNDER/`. The first run may ask for your Ubuntu password to install build tools. It downloads and caches the pinned dependencies, generates the game code locally, and prints the path to the finished package under `out/`. The first build can take a while.

## Manual build

Use Ubuntu 24.04 or Ubuntu 24.04 under WSL2. Keep the checkout and build directories on the Linux filesystem for faster builds. Run the following from the repository root.

```bash
sudo apt install build-essential cmake ninja-build clang lld llvm curl nasm \
  python3 python3-capstone python3-pefile python3-numpy
mkdir -p "$HOME/ps4/sdk"
```

Download the pinned [orbis-sdk-v1 bundle](https://github.com/orbis-ports/orbis-porting-kit/releases/tag/orbis-sdk-v1) and verify the archive before extracting it.

```bash
echo 'f4f079a4df334b46a76996f895283904283d6a006f97102aafaa53d4ecfc9622  orbis-sdk-v1.tar.gz' | sha256sum -c - &&
  tar xzf orbis-sdk-v1.tar.gz -C "$HOME/ps4/sdk"
```

Use the helper scripts on a fresh toolchain setup. They fetch OpenOrbis v0.5.4, glslang 15.4.0 and its pinned SPIR-V dependencies, and FFmpeg n7.1. The FFmpeg build enables VP6 decoding for the game's movies.

```bash
bash tools/get-oo.sh
bash tools/get-glslang.sh
bash tools/get-spirv.sh
bash tools/build-glslang.sh
bash tools/get-ffmpeg.sh
bash nfsu2-ps4/ps4/build_ffmpeg_vp6.sh
```

Set `NFSU2_XBE` to your extracted disc's executable. The analysis file and generated C stay on your machine. The source is based on nfsu2-sw revision `3247829db250e871b398dd6cfe931d8068332027`.

```bash
export NFSU2_XBE=/absolute/path/to/game/default.xbe
export NFSU2_GEN_DIR="$HOME/nfsu2x/gen"
cd nfsu2-ps4
(
  cd xboxrecomp
  python3 -m tools.xbe_parser "$NFSU2_XBE" --json "${NFSU2_XBE%.*}_analysis.json"
)
bash tools/regen.sh
bash ps4/build.sh
```

To make a local package, point `OPENSSL_LIB` to the directory containing the OpenSSL 1.1 libraries needed by the OpenOrbis v0.5.4 packaging tool. The build previously used the libraries extracted from Ubuntu's `libssl1.1_1.1.1-1ubuntu2.1~18.04.23_amd64.deb` (SHA-256 `986727f5a99088bef98971edfcd9567f5693b5e941aa241894fce1020c7015a0`).

```bash
OPENSSL_LIB=/absolute/path/to/openssl11/lib bash ps4/build.sh pkg
```

The package is written to `~/ps4/out-nfsu2/` by default. The standard recipe uses the OpenOrbis sample icon. The package contains code generated from your own Xbox executable and pulls packaging modules from your local OpenOrbis setup. Keep the resulting package for local use.

Once the build finishes, install the package you made and follow the [game file setup in the README](README.md#download-and-install).
