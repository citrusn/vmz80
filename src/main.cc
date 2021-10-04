#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "z80.cc"
#include "machine.h"
#include "machine.cc"
#include "constructor.cc"
#include "video.cc"
#include "ay.cc"
#include "io.cc"
#include "snapshot.cc"
#include "disasm.cc"

// Расширения
#include "addon.spi.cc"

int main(int argc, char** argv) {        
    Z80Spectrum speccy;

    speccy.args(argc, argv);
    speccy.main();

    return 0;
}
