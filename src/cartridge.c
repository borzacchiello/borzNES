#include "cartridge.h"
#include "logging.h"
#include "alloc.h"

#include <stdio.h>
#include <string.h>

#define HEADER_SIZE  16u
#define TRAINER_SIZE 512u

#define MIRRORING_MASK  (uint8_t)(1 << 0)
#define BATTERY_MASK    (uint8_t)(1 << 1)
#define TRAINER_MASK    (uint8_t)(1 << 2)
#define MIRROR_ALL_MASK (uint8_t)(1 << 3)

typedef struct {
    uint8_t* buffer;
    uint64_t size;
} Buffer;

static Buffer read_file_raw(const char* path)
{
    FILE* fileptr = fopen(path, "rb");
    if (fileptr == NULL)
        panic("unable to open the file %s", path);

    if (fseek(fileptr, 0, SEEK_END) < 0)
        panic("fseek failed");

    long filelen = ftell(fileptr);
    if (filelen < 0)
        panic("ftell failed");

    rewind(fileptr);

    uint8_t* buffer = malloc(filelen * sizeof(char));
    if (fread(buffer, 1, filelen, fileptr) != filelen)
        panic("fread failed");

    fclose(fileptr);

    Buffer res = {.buffer = buffer, .size = filelen};
    return res;
}

Cartridge* cartridge_load(const char* path)
{
    Buffer raw = read_file_raw(path);
    if (raw.size < HEADER_SIZE)
        panic("not a valid cartridge");

    if (memcmp(raw.buffer, "NES\x1a", 4) != 0)
        panic("not a valid cartridge (wrong magic)");

    Cartridge* cart = malloc_or_fail(sizeof(Cartridge));
    cart->fpath     = strdup(path);

    cart->PRG_size = (uint32_t)raw.buffer[4] << 14u;
    cart->CHR_size = (uint32_t)raw.buffer[5] << 13u;

    uint8_t flag_6  = raw.buffer[6];
    uint8_t flag_7  = raw.buffer[7];
    uint8_t flag_8  = raw.buffer[8];
    uint8_t flag_9  = raw.buffer[9];
    uint8_t flag_10 = raw.buffer[10];

    if ((flag_6 & MIRRORING_MASK) == 0)
        cart->mirror = MIRROR_HORIZONTAL;
    else
        cart->mirror = MIRROR_VERTICAL;
    if (flag_6 & MIRROR_ALL_MASK)
        cart->mirror = MIRROR_ALL;

    cart->battery = (flag_6 & BATTERY_MASK) > 0;
    cart->mapper  = (flag_6 >> 4) | (flag_7 & 0xf);

    if (flag_8 == 0)
        flag_8 = 1;
    cart->SRAM_size = (uint32_t)flag_8 << 13;
    cart->SRAM      = malloc_or_fail(cart->SRAM_size);

    uint32_t file_off = HEADER_SIZE;
    if (flag_6 & TRAINER_MASK) {
        if (raw.size - file_off < TRAINER_SIZE)
            panic("not a valid cartridge (trainer truncated)");

        cart->trainer = malloc_or_fail(TRAINER_SIZE);
        memcpy(cart->trainer, raw.buffer + file_off, TRAINER_SIZE);
        file_off += TRAINER_SIZE;
    } else
        cart->trainer = NULL;

    if (file_off + cart->PRG_size > raw.size)
        panic("not a valid cartridge (PRG truncated)");
    cart->PRG = malloc_or_fail(cart->PRG_size);
    memcpy(cart->PRG, raw.buffer + file_off, cart->PRG_size);
    file_off += cart->PRG_size;

    if (cart->CHR_size == 0) {
        // allocate CHR-RAM
        cart->CHR      = calloc_or_fail(1 << 13);
        cart->CHR_size = 1 << 13;
    } else {
        if (file_off + cart->CHR_size > raw.size)
            panic("not a valid cartridge (CHR truncated)");

        cart->CHR = malloc_or_fail(cart->CHR_size);
        memcpy(cart->CHR, raw.buffer + file_off, cart->CHR_size);
        file_off += cart->CHR_size;
    }

    if (file_off != raw.size)
        panic("not a valid cartridge (unread data at the end)");

    free(raw.buffer);
    return cart;
}

void cartridge_unload(Cartridge* cart)
{
    free(cart->trainer);
    free(cart->PRG);
    free(cart->CHR);
    free(cart->SRAM);
    free(cart->fpath);
    free(cart);
}

void cartridge_print(Cartridge* cart)
{
    printf("Cartridge @ \"%s\" {\n", cart->fpath);
    printf("  PRG_size:  %u\n", cart->PRG_size);
    printf("  CHR_size:  %u\n", cart->CHR_size);
    printf("  mapper:    %u\n", cart->mapper);
    printf("  battery:   %s\n", cart->battery ? "true" : "false");
    printf("  mirroring: %s\n",
           cart->mirror == MIRROR_HORIZONTAL
               ? "horizontal"
               : (cart->mirror == MIRROR_VERTICAL ? "vertical" : "all"));
    printf("}\n");
}
