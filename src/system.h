#ifndef SYSTEM_H
#define SYSTEM_H

#include <stdint.h>

#define CPU_1X_FREQ   1789773l
#define CPU_2X_FREQ   (2l * CPU_1X_FREQ)
#define CPU_0_5X_FREQ (CPU_1X_FREQ / 2l)

struct Cpu;
struct Ppu;
struct Apu;
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
    struct Apu*       apu;
    struct Cartridge* cart;
    struct Mapper*    mapper;
    char*             state_save_path;
    uint8_t           RAM[2048];
    ControllerState   controller_state[2];
    uint8_t           controller_shift_reg[2];
    int64_t           cpu_freq;
} System;

System* system_build(const char* rom_path);
void    system_destroy(System* sys);

uint64_t system_step(System* sys);
void     system_step_ms(System* sys, int64_t delta_time);

void    system_update_controller(System* sys, ControllerNum num,
                                 ControllerState state);
void    system_load_controllers(System* sys);
uint8_t system_get_controller_val(System* sys, ControllerNum num);

// It returns a local buffer that will be invalidated
// if the function is called again
const char* system_tostring(System* sys);

void system_save_state(System* sys, const char* path);
void system_load_state(System* sys, const char* path);

#endif
