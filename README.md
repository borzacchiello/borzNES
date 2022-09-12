# borzNES

Yet another NES emulator, written just for fun :)

No APU (audio) and no save (yet).

![screencast](img/screencast.gif)

### Build

Install SDL2 and SDL2_ttf and build using cmake:
```
$ mkdir build
$ cd build
$ cmake -DCMAKE_BUILD_TYPE=Release ../src
$ make
```

Tested on MacOS and Ubuntu.

### Usage

From the build directory:

```
$ ./borznes /path/to/rom
```

beware that it expects "courier.ttf" in the pwd (yeah, it's ugly).

### Keymappings

The (hardcoded) keymappings for Player1 are the following:

| action                           | key        |
|----------------------------------|------------|
| A                                |  Z         |
| B                                |  X         |
| Start                            | Enter      |
| Select                           | RightShift |
| Up/Down/Left/Right               | ArrowKeys  |
| Fast Forward Mode (2x CPU speed) | F          |
| Change Nametable Palette         | P          |
| Quit                             | Q          |

### Implemented Mappers

NROM(000), MMC1(001), MMC3(004)
