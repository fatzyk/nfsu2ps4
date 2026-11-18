# Need for Speed: Underground 2 on PS4

I love NFSU2 and my old ps4, but the ps2 version via the native emulator runs like ass. So I've decided make an unofficial native PS4 homebrew port based on [nfsu2-sw](https://github.com/antoxa2584x/nfsu2-sw), the static recompilation of the original Xbox release. The port has been tested on PS4 FAT running firmware 11.00 with GoldHEN.

I get good performance with 30 fps cap in 1.5x resolution and all other settings maxed. Other PS4 models and firmware versions have not been verified, but should be even better.
## Download and install

1. Download the PS4 package (.pkg) from [GitHub Releases](https://github.com/fatzyk/nfsu2ps4/releases) and install it with GoldHEN's Package Installer.
2. Copy your full extracted **original Xbox NTSC-U (USA)** game to **/data/nfsu2x/game/** on the PS4. Put **default.xbe** and the **NFSUNDER/** folder directly inside it.
3. Launch the game from the PS4 home screen.

Saves are stored in **/data/nfsu2x/save/**.

## Optional: build it yourself

Follow the [build guide](BUILDING.md) to make your own package, then use the same installation steps above with your locally built file.

## Credits

Thanks to antoxa2584x for nfsu2-sw, the xboxrecomp contributors, and the orbis-ports, OpenOrbis, Mesa, glslang, SPIR-V and FFmpeg teams. The vendored xboxrecomp licence and component notices are in [LICENSE](https://github.com/fatzyk/nfsu2ps4/blob/main/nfsu2-ps4/xboxrecomp/LICENSE) and [LICENSES](https://github.com/fatzyk/nfsu2ps4/blob/main/nfsu2-ps4/xboxrecomp/LICENSES).
