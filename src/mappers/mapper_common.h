#ifndef MAPPER_COMMON_H
#define MAPPER_COMMON_H

#include "../mapper.h"
#include "../alloc.h"
#include "../cartridge.h"
#include "../logging.h"
#include "../ppu.h"
#include "../system.h"
#include "../memory.h"
#include "../6502_cpu.h"

#include <assert.h>
#include <string.h>

#define check_inbound(addr, size)                                              \
    if ((addr) < 0 || (addr) >= (size))                                        \
        panic("check_inbound failed");

// http://tuxnes.sourceforge.net/mappers-0.80.txt

static void __attribute__((unused)) do_nothing_fun(void* v) { (void)v; }

#define GEN_SERIALIZER(TYPE)                                                   \
    void TYPE##_serialize(void* _map, FILE* fout)                              \
    {                                                                          \
        Buffer res = {.buffer = (uint8_t*)_map, .size = sizeof(TYPE)};         \
        dump_buffer(&res, fout);                                               \
    }
#define GEN_DESERIALIZER_WITH_POSTCHECK(TYPE, post_check_fun)                  \
    void TYPE##_deserialize(void* _map, FILE* fin)                             \
    {                                                                          \
        Buffer buf = read_buffer(fin);                                         \
        if (buf.size != sizeof(TYPE))                                          \
            panic(#TYPE "_deserialize(): invalid buffer");                     \
        TYPE* map      = (TYPE*)_map;                                          \
        void* tmp_cart = map->cart;                                            \
        memcpy(map, buf.buffer, buf.size);                                     \
        map->cart = tmp_cart;                                                  \
        post_check_fun((void*)map);                                            \
        free_or_fail(buf.buffer);                                              \
    }
#define GEN_DESERIALIZER(TYPE)                                                 \
    GEN_DESERIALIZER_WITH_POSTCHECK(TYPE, do_nothing_fun)

int32_t calc_prg_bank_offset(Cartridge* cart, int32_t idx, uint32_t bank_size);
int32_t calc_chr_bank_offset(Cartridge* cart, int32_t idx, uint32_t bank_size);
int32_t calc_ram_bank_offset(Cartridge* cart, int32_t idx, uint32_t bank_size);

#endif
