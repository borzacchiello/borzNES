#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

struct Cpu;
struct Ppu;
struct Cartridge;
struct Mapper;

typedef enum { P1 = 0, P2 = 1 } ControllerNum;

typedef struct ControllerState {
    union {
        struct {
            // LSB
            uint8_t A : 1;
            uint8_t B : 1;
            uint8_t SELECT : 1;
            uint8_t START : 1;
            uint8_t UP : 1;
            uint8_t DOWN : 1;
            uint8_t LEFT : 1;
            uint8_t RIGHT : 1;
            // MSB
        };
        uint8_t state;
    };
} ControllerState;

typedef struct System {
    struct Cpu*       cpu;
    struct Ppu*       ppu;
    struct Cartridge* cart;
    struct Mapper*    mapper;
    uint8_t           RAM[2048];
    ControllerState   controller_state[2];
    uint8_t           controller_shift_reg[2];
} System;

System* system_build(const char* rom_path);
void    system_destroy(System* sys);

uint64_t system_step(System* sys);

void    system_update_controller(System* sys, ControllerNum num,
                                 ControllerState state);
void    system_load_controllers(System* sys);
uint8_t system_get_controller_val(System* sys, ControllerNum num);

// It returns a local buffer that will be invalidated
// if the function is called again
const char* system_tostring(System* sys);

#endif
