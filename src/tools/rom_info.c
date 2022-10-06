#include "../cartridge.h"

#include <stdio.h>
#include <stdlib.h>

static void usage(const char* prg)
{
    fprintf(stderr, "USAGE: %s <rom>\n", prg);
    exit(1);
}

int main(int argc, char const* argv[])
{
    if (argc != 2)
        usage(argv[0]);

    Cartridge* cart = cartridge_load(argv[1]);
    cartridge_print(cart);

    cartridge_unload(cart);
    return 0;
}
