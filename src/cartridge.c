#include "cartridge.h"
#include "logging.h"
#include "alloc.h"

#include <unistd.h>
#include <string.h>

#define HEADER_SIZE  16u
#define TRAINER_SIZE 512u

#define MIRRORING_MASK  (uint8_t)(1 << 0)
#define BATTERY_MASK    (uint8_t)(1 << 1)
#define TRAINER_MASK    (uint8_t)(1 << 2)
#define MIRROR_ALL_MASK (uint8_t)(1 << 3)

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

    uint8_t* buffer = malloc_or_fail(filelen * sizeof(char));
    if (fread(buffer, 1, filelen, fileptr) != filelen)
        panic("fread failed");

    fclose(fileptr);

    Buffer res = {.buffer = buffer, .size = filelen};
    return res;
}

static void write_file_raw(const char* path, Buffer* data)
{
    FILE* fileptr = fopen(path, "wb");
    if (fileptr == NULL)
        panic("unable to open the file %s", path);

    if (fwrite(data->buffer, 1, data->size, fileptr) != data->size)
        panic("fwrite failed");

    fclose(fileptr);
}

static char* get_sav_path(const char* fpath)
{
    size_t fpath_size = strlen(fpath);
    char*  res        = calloc_or_fail(fpath_size + 5);
    strcpy(res, fpath);
    strcat(res, ".sav");

    return res;
}

Cartridge* cartridge_load_from_buffer(Buffer raw)
{
    if (raw.size < HEADER_SIZE)
        panic("not a valid cartridge");

    if (memcmp(raw.buffer, "NES\x1a", 4) != 0)
        panic("not a valid cartridge (wrong magic)");

    Cartridge* cart = malloc_or_fail(sizeof(Cartridge));
    cart->fpath     = NULL;
    cart->sav_path  = NULL;

    cart->PRG_size = (uint32_t)raw.buffer[4] << 14u;
    cart->CHR_size = (uint32_t)raw.buffer[5] << 13u;

    uint8_t flag_6  = raw.buffer[6];
    uint8_t flag_7  = raw.buffer[7];
    uint8_t flag_8  = raw.buffer[8];
    uint8_t flag_9  = raw.buffer[9];
    uint8_t flag_10 = raw.buffer[10];

    // UNUSED, todo
    (void)flag_9;
    (void)flag_10;

    uint8_t mirror_bit1 = (flag_6 & MIRRORING_MASK) ? 1 : 0;
    uint8_t mirror_bit2 = (flag_6 & MIRROR_ALL_MASK) ? 1 : 0;
    cart->mirror        = mirror_bit1 | (mirror_bit2 << 1);

    cart->battery = (flag_6 & BATTERY_MASK) > 0;
    cart->mapper  = (flag_6 >> 4) | (flag_7 & 0xf0);

    if (flag_8 == 0)
        flag_8 = 1;
    cart->SRAM_size = (uint32_t)flag_8 << 13;
    if (cart->SRAM_size > raw.size)
        panic("Invalid SRAM_size");
    cart->SRAM = malloc_or_fail(cart->SRAM_size);

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

    return cart;
}

Cartridge* cartridge_load(const char* path)
{
    Buffer raw = read_file_raw(path);

    Cartridge* cart = cartridge_load_from_buffer(raw);
    cart->fpath     = strdup(path);
    cart->sav_path  = cart->battery ? get_sav_path(path) : NULL;

    cartridge_load_sav(cart);
    free(raw.buffer);
    return cart;
}

void cartridge_unload(Cartridge* cart)
{
    cartridge_save_sav(cart);

    free(cart->trainer);
    free(cart->PRG);
    free(cart->CHR);
    free(cart->SRAM);
    free(cart->fpath);
    free(cart->sav_path);
    free(cart);
}

void cartridge_load_sav(Cartridge* cart)
{
    if (!cart->sav_path)
        return;

    if (access(cart->sav_path, F_OK))
        // The file does not exist
        return;

    Buffer ram = read_file_raw(cart->sav_path);
    if (ram.size != cart->SRAM_size)
        panic("invalid sav file");

    memcpy(cart->SRAM, ram.buffer, ram.size);
    free(ram.buffer);
}

void cartridge_save_sav(Cartridge* cart)
{
    if (!cart->sav_path)
        return;

    Buffer buf = {.buffer = cart->SRAM, .size = cart->SRAM_size};
    write_file_raw(cart->sav_path, &buf);
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

void cartridge_serialize(Cartridge* cart, FILE* ofile)
{
    Buffer buf;
    buf.buffer = cart->SRAM;
    buf.size   = cart->SRAM_size;
    dump_buffer(&buf, ofile);

    buf.buffer = cart->CHR;
    buf.size   = cart->CHR_size;
    dump_buffer(&buf, ofile);

    buf.buffer = (uint8_t*)&cart->mirror;
    buf.size   = sizeof(cart->mirror);
    dump_buffer(&buf, ofile);
}

void cartridge_deserialize(Cartridge* cart, FILE* ifile)
{
    Buffer buf = read_buffer(ifile);
    if (buf.size != cart->SRAM_size)
        panic("cartridge_deserialize(): invalid buffer SRAM");
    memcpy(cart->SRAM, buf.buffer, buf.size);
    free(buf.buffer);

    buf = read_buffer(ifile);
    if (buf.size != cart->CHR_size)
        panic("cartridge_deserialize(): invalid buffer CHR");
    memcpy(cart->CHR, buf.buffer, buf.size);
    free(buf.buffer);

    buf = read_buffer(ifile);
    if (buf.size != sizeof(cart->mirror))
        panic("cartridge_deserialize(): invalid buffer Mirror");
    cart->mirror = *(Mirroring*)buf.buffer;
    free(buf.buffer);
}
