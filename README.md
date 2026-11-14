# Need for Speed: Underground 2 on PS4

An unofficial, experimental native PS4 homebrew port based on [nfsu2-sw](https://github.com/antoxa2584x/nfsu2-sw), the static recompilation of the original Xbox release. You build it locally using your own extracted **NTSC-U (USA) original Xbox disc**, with `default.xbe` and the `NFSUNDER/` directory.

The port has been tested on a launch-model PS4 running firmware 11.00 with GoldHEN and a DualShock 4. The November console builds ran races with a 30 FPS cap, played music and speech, and passed a Home, resume and Close Application test. Performance varies by scene. Other PS4 models and firmware versions have not been verified.

## Build and install

Follow the [build guide](BUILDING.md) to generate the game code locally and create an installable PS4 package. Copy the complete extracted Xbox disc to `/data/nfsu2x/game/` on the PS4 so that `default.xbe` is directly inside that folder. Install the locally built package with GoldHEN’s Package Installer, then launch it from Home.

Saves are stored in /data/nfsu2x/save/.

For Switch builds, see the upstream project. This PS4 source distribution omits its Switch artwork and generated logo header.

## Credits

The PS4 port was developed and tested by Stas (fatzyk) with AI coding assistance. Thanks to antoxa2584x for nfsu2-sw, the xboxrecomp contributors, and the orbis-ports, OpenOrbis, Mesa, glslang, SPIR-V and FFmpeg teams. The vendored xboxrecomp licence and component notices are in [LICENSE](nfsu2-sw/xboxrecomp/LICENSE) and [LICENSES](nfsu2-sw/xboxrecomp/LICENSES/).
