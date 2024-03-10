# sdl-ogc-keyboard

An on-screen keyboard (OSK) for Nintendo GameCube and Wii homebrew applications
using libSDL. Features:

* Invoked using the usual
  [`SDL_StartTextInput()`](https://wiki.libsdl.org/SDL2/SDL_StartTextInput) API
* Panning of application's window to ensure visibility of the input field
* Use own input field if the application didn't specify one
* Four selectable layout layers
* Vibration when hovering on keys
* Low memory impact: layout textures weight less than 64KB, UI is built using
  fillrect GX operations
* Wiimote support
* Customizable font


## How to build

The key symbols on the OSK are drawn as textures, generated using the `SDL_ttf`
library. While this generation could theoretically be done on the console
itself, doing so freezes the application for several seconds, so we are instead
following an approach where the textures are pre-generated on the host machine
using the `ogc-osk-tool` found in the `tools/` directory. Therefore, the first
step in order to build sdl-ogc-keyboard is to build the tool on the host.


### Build the ogc-osk-tool

**Dependencies**: The project uses the [cmake](http://cmake.org) build system,
`SDL` 2.0 and `SDL_ttf` 2.0. They can all be installed on a Debian-based
distribution using the following command:

    apt install cmake libsdl2-dev libsdl2-ttf-dev

**Build commands**:

    cmake -S. -Bhostbuild
    cd hostbuild
    make

The `ogc-osk-tool` will then be found in the `hostbuild/tools/` directory.


### Generate the layout data

Pick your favourite TTF font (the `DejaVuSans.ttf` in this repository is public
domain) and run the generator tool:

    # assuming you are still inside the `hostbuild` directory:
    ./tools/ogc-osk-tool ../example/DejaVuSans.ttf 24

(24 being the font size). Remember to copy the generated `osk*.tex` files to
the directory where your application's data are: `sdl-ogc-keyboard` expects to
find them in the current working directory.


### Build sdl-ogc-keyboard

NOTE: the instructions below will only apply once [this pull
request](https://github.com/devkitPro/SDL/pull/61) will have been merged!

**Dependencies**: the [devkitPro](https://devkitpro.org/) toolchain and the
SDL2 package installable from the devkitPro repositories.

**Build commands**:

    cmake -S. -Bbuild -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Wii.cmake
    cd build
    make

If building for the GameCube, replace `Wii.cmake` with `Cube.cmake`.


## Using sdl-ogc-keyboard in your application

Enabling the OSK is a matter of changing a few lines only:

1. Include the header file:

    #include "ogc_keyboard.h"

2. Activate the module (this can be done even before calling `SDL_Init()`):

    SDL_OGC_RegisterVkPlugin(ogc_keyboard_get_plugin());

3. Call `SDL_StartTextInput()` when needed.


## Configuring the layouts

Layouts can be configured by editing `src/config.c`.


## Future plans

- Provide a way to embed the layout data in the application
- More flexible layout design (now it's hardcoded to 4 layouts, each with 5
  rows)
- Read layouts from config files
- Play sounds as keys are pressed
