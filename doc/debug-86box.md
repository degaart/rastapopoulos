# How to debug in 86box

## Compile 86box with debugger support

```sh
git clone --depth=1 https://github.com/86Box/86Box.git
sudo apt install libsndfile1-dev libslirp-dev libopenal-dev \
    qtbase5-dev qtbase5-dev-tools qttools5-dev libsdl3-dev \
    qtbase5-private-dev
cmake --preset optimized  \
    -G Ninja -B build -L \
    -DRTMIDI=OFF -DFLUIDSYNTH=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=$HOME/Applications/86box \
    -DGDBSTUB=on
cmake --build build
cmake --build build --target install
```

## Run 86box

```sh
cd $HOME/Applications/86box

# ... Unzip roms into bin/roms

86box

# ... Configure 86box
# ... run the VM. It will stop until a debugger is attached
```

## Run gdb

```sh
gdb
```

### gdb initialization commands

```
set architecture i8086
set disassembly-flavor intel
layout asm
target remote localhost:12345
hbreak *0x7c00
```

## Useful gdb commands

- `si`: step into asm instruction
- `ni`: step over asm instruction
- `info registers`




