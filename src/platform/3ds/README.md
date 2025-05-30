# Zelda3 on 3DS

## Building

First you need to extract the assets from the ROM, same as you would for the PC
version. See the top-level instructions for more details.

To summarize:

1. Copy the ROM file (`zelda3.sfc`) to the top level folder.

2. Run the python script to extract the game files:

```sh
python assets/restool.py --extract-from-rom
```

3. This should create the file `zelda3_assets.dat` - copy this file to the `romfs` folder:

```sh
cp zelda3_assets.dat src/platform/3ds/romfs/.
```

4. Run the `make` command, which should create the output file `zelda3.3dsx`

You can run this file in your favorite emulator, or use a modded 3DS to install
and run the game.


## TODOs

In order of importance:

* BUG: Game runs at half speed (or slower) due to slow render time
* FET: Saving not yet supported
* FET: No audio / music
* FET: Support for widescreen mode
* BUG: Enhanced mode 7 causes the game to crash, so it must be disabled.
