# Building TTK with CMake

This document describes how to build the TTK library and examples using the CMake build system.

## Prerequisites

*   **CMake**: Version 3.5 or later.
*   **Compiler**: GCC/G++ or a suitable cross-compiler for iPod.
*   **Flex**: Required to generate the lexer from `appearance.l`. If Flex is not found, the build system expects a pre-generated `lex.yy.c` to exist.
*   **Dependencies**: Depending on the `GFXLIB` selected, you may need SDL 1.2, libpng, libjpeg, or Microwindows/Nano-X.

## Configuration Options

You can configure the build by passing `-D<OPTION>=<VALUE>` to the `cmake` command.

| Option | Default | Description |
| :--- | :--- | :--- |
| `GFXLIB` | `SDL` | Selects the graphics backend. Supported values: `SDL`, `hotdog`, `mwin`. |
| `DEBUG` | `OFF` | Enables debug symbols (`-g`) and disables optimizations. |
| `IPOD` | `OFF` | Configures the build for the iPod environment (defines `-DIPOD`, adjusts library paths). |
| `TTF` | `ON` | Enables TrueType Font support. Set to `OFF` to define `-DNO_TF`. |
| `BUILD_LNDIR` | `OFF` | Builds the `lndir` utility (legacy build helper). |

## Building for Desktop (Linux/macOS)

The default build uses the SDL backend, which is ideal for testing on a desktop environment.

1.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```

2.  Run CMake:
    ```bash
    cmake ..
    ```
    *To use a different backend, e.g., hotdog:*
    ```bash
    cmake -DGFXLIB=hotdog ..
    ```

3.  Compile:
    ```bash
    make
    ```

This will generate the static library `libttk-<GFXLIB>.a` and the example executables (`exscroll`, `exmenu`, `eximage`, `exti`).

## Building for iPod (Cross-Compilation)

To build for the iPod, you must enable the `IPOD` option and specify your cross-compiler.

1.  Create a build directory:
    ```bash
    mkdir build-ipod
    cd build-ipod
    ```

2.  Run CMake specifying the toolchain (replace compilers with your specific binary names):
    ```bash
    cmake -DIPOD=ON \
          -DCMAKE_C_COMPILER=arm-uclinux-elf-gcc \
          -DCMAKE_CXX_COMPILER=arm-uclinux-elf-g++ \
          ..
    ```

### Dependency Layout for iPod
When `IPOD` is enabled, the build script assumes a specific directory layout relative to the `ttk/src` directory to find static libraries:
*   **Common Libs**: `../libs/common/*.a`
*   **SDL Libs**: `../libs/SDL/*.a` (if GFXLIB is SDL)
*   **Microwindows**: `../libs/mwin/*.a` (if GFXLIB is mwin)
*   **Hotdog**: `../../../hotdog/ipod/libhotdog.a` (if GFXLIB is hotdog)