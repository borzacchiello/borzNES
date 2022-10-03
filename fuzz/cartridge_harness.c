#include "../src/cartridge.h"

#include "common.h"

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
    Buffer buf = {.buffer = Data, .size = Size};

    int result = setjmp(env);
    if (result != 0)
        return 1;

    Cartridge* cart = cartridge_load_from_buffer(buf);
    cartridge_unload(cart);
    return 0;
}

#ifdef AFL
int main(int argc, char const* argv[])
{
    if (argc < 2)
        return 1;

    Buffer b = read_file_raw(argv[1]);
    return LLVMFuzzerTestOneInput(b.buffer, b.size);
}
#endif
