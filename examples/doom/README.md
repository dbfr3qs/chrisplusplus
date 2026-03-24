# DOOM — Running on chrisplusplus

This is a full port of DOOM (shareware) running through the chrisplusplus compiler.

## Architecture

```
doom.chr (chrisplusplus)  →  doomgeneric (C engine)  →  SDL2 (rendering)
         extern func            libdoom.a                 brew install sdl2
```

- **`doom.chr`** — chrisplusplus entry point, calls `dg_start()` via `extern func`
- **`dg_shim.c`** — Implements 6 SDL2 platform functions required by doomgeneric
- **`doomgeneric/`** — Unmodified C Doom engine (github.com/ozkl/doomgeneric)
- **`doom1.wad`** — Doom shareware WAD (freely distributable)

## Build & Run

```bash
# 1. Install SDL2
brew install sdl2

# 2. Build doomgeneric + shim
bash examples/doom/build_doom.sh

# 3. Build with chrisplusplus
./build/chris build examples/doom/doom.chr \
    examples/doom/build/libdoom.a \
    examples/doom/build/dg_shim.o \
    -L/opt/homebrew/lib -lSDL2

# 4. Run
cd examples/doom && ./doom
```

## Controls

| Key | Action |
|-----|--------|
| Arrow keys | Move / Turn |
| Ctrl | Fire |
| Space | Open doors / Use |
| Shift | Run |
| Escape | Menu |
| 1-7 | Select weapon |
