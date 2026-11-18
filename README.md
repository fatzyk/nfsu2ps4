# Need for Speed: Underground 2 on PS4

I love NFSU2 and my old ps4, but the ps2 version runs like ass. So I've decided make an unofficial native PS4 homebrew port based on [nfsu2-sw](https://github.com/antoxa2584x/nfsu2-sw), the static recompilation of the original Xbox release. The port has been tested on PS4 FAT running firmware 11.00 with GoldHEN.

I get good performance with 30 fps cap in 1.5x resolution and all other settings maxed. Other PS4 models and firmware versions have not been verified, but should be even better. You build it locally using your own extracted NTSC-U (USA) original Xbox disc, with default.xbe and the NFSUNDER/ directory.


## Build and install
Install the FPKG or build yourself by following the [build guide](BUILDING.md) to generate the game code locally and create an installable PS4 package.
In both cases copy the complete extracted Xbox disc to `/data/nfsu2x/game/` on the PS4 so that `default.xbe` is directly inside that folder.

Saves are stored in /data/nfsu2x/save/.

## Credits

Thanks to antoxa2584x for nfsu2-sw, the xboxrecomp contributors, and the orbis-ports, OpenOrbis, Mesa, glslang, SPIR-V and FFmpeg teams. The vendored xboxrecomp licence and component notices are in [LICENSE](https://github.com/fatzyk/nfsu2ps4/blob/main/nfsu2-sw/xboxrecomp/LICENSE) and [LICENSES](https://github.com/fatzyk/nfsu2ps4/blob/main/nfsu2-sw/xboxrecomp/LICENSES).
