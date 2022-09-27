#ifndef APU_H
#define APU_H

#include <SDL2/SDL.h>
#include <stdint.h>

struct System;
struct Cpu;

typedef struct {
    float prev_x;
    float prev_y;
} FilterState;

typedef struct {
    int         sample_rate;
    FilterState high_90;
    FilterState high_440;
    FilterState low_14000;
} SoundFilter;

typedef struct Pulse {
    union {
        struct {
            uint8_t duty_cycle : 2;
            uint8_t length_counter_halt : 1;
            uint8_t constant_volume_envelope_flag : 1;
            uint8_t volume_driver_period : 4;
        };
        uint8_t control_flags;
    };

    union {
        struct {
            uint8_t sweep_enabled : 1;
            uint8_t sweep_period : 3;
            uint8_t sweep_negate : 1;
            uint8_t sweep_shift : 3;
        };
        uint8_t sweep_flags;
    };

    union {
        struct {
            uint8_t enabled : 1;
            uint8_t envelope_start : 1;
            uint8_t sweep_reload : 1;
            uint8_t channel : 1;
        };
        uint8_t other_flags;
    };

    uint8_t  envelope_volume;
    uint8_t  envelope_value;
    uint8_t  duty_value;
    uint8_t  sweep_value;
    uint8_t  length_value;
    uint16_t timer_value;
    uint16_t timer_period;
} Pulse;

typedef struct Triangular {
    union {
        struct {
            uint8_t enabled : 1;
            uint8_t length_counter_halt : 1;
            uint8_t counter_reload : 1;
        };
        uint8_t flags;
    };

    uint8_t  counter_value;
    uint8_t  counter_period;
    uint8_t  length_value;
    uint8_t  duty_value;
    uint16_t timer_value;
    uint16_t timer_period;
} Triangular;

typedef struct Noise {
    union {
        struct {
            uint8_t enabled : 1;
            uint8_t length_counter_halt : 1;
            uint8_t constant_volume_envelope_flag : 1;
            uint8_t volume_driver_period : 4;
            uint8_t envelope_start : 1;
            uint8_t mode : 1;
        };
        uint16_t flags;
    };

    uint16_t shift_reg;
    uint8_t  envelope_volume;
    uint8_t  envelope_value;
    uint16_t timer_value;
    uint16_t timer_period;
    uint8_t  length_value;
} Noise;

typedef struct DMC {
    union {
        struct {
            uint8_t enabled : 1;
            uint8_t loop : 1;
            uint8_t irq : 1;
        };
        uint16_t flags;
    };

    struct System* sys;
    uint8_t        value;
    uint16_t       sample_addr;
    uint16_t       sample_length;
    uint16_t       current_addr;
    uint16_t       current_length;
    uint8_t        shift_register;
    uint8_t        bit_count;
    uint8_t        tick_period;
    uint8_t        tick_value;
} DMC;

typedef struct Apu {
    struct System*    sys;
    SDL_AudioDeviceID dev;
    SDL_AudioSpec     spec;

    union {
        struct {
            uint8_t frame_period_mode : 1;
            uint8_t frame_irq : 1;
            uint8_t is_paused : 1;
        };
        uint8_t flags;
    };

    SoundFilter filter;

    Pulse      pulse1;
    Pulse      pulse2;
    Triangular triangular;
    Noise      noise;
    DMC        dmc;

    uint8_t  frame_value;
    float*   sound_buffer;
    uint32_t sound_buffer_num_els;
    uint32_t sound_buffer_i;
    uint64_t cycles;
} Apu;

Apu* apu_build(struct System* sys);
void apu_destroy(Apu* apu);

void apu_step(Apu* apu);
void apu_write_register(Apu* apu, uint16_t addr, uint8_t value);

void apu_pause(Apu* apu);
void apu_unpause(Apu* apu);

uint32_t apu_get_queued(Apu* apu);

#endif
