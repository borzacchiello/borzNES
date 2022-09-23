#include "apu.h"
#include "logging.h"
#include "alloc.h"
#include "system.h"
#include "6502_cpu.h"
#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

#define ENABLE_HIGH_FILTER_1 1
#define ENABLE_HIGH_FILTER_2 1
#define ENABLE_LOW_FILTER    1

#define ENABLE_PULSE1 1
#define ENABLE_PULSE2 1
#define ENABLE_TRIANG 1
#define ENABLE_NOISE  1
#define ENABLE_DMC    1

#define ENVELOPE_LOOP(wave)  ((wave)->length_counter_halt == 1)
#define LENGTH_ENABLED(wave) ((wave)->length_counter_halt == 0)

static float __attribute__((unused)) approx_sine(float x)
{
    // https://www.youtube.com/watch?v=1xlCVBIF_ig
    float t = x * 0.15915f;
    t       = t - (int)t;
    if (t < 0.5)
        return (-16.0f * t * t) + (8.0f * t);
    return (16.0f * t * t) - (16.0f * t) - (8.0f * t) + 8.0f;
}

static uint8_t duty_tab[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1},
};

static uint8_t length_tab[] = {
    10, 254, 20, 2,  40, 4,  80, 6,  160, 8,  60, 10, 14, 12, 26, 14,
    12, 16,  24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

static uint8_t triangle_tab[] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5,  4,  3,  2,  1,  0,
    0,  1,  2,  3,  4,  5,  6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

static uint16_t noise_tab[] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

static uint8_t dmc_tab[] = {
    214, 190, 170, 160, 143, 127, 113, 107, 95, 80, 71, 64, 53, 42, 36, 27,
};

static float pulse_tab[31];
static float tnd_tab[203];

__attribute__((constructor)) static void init_tabs()
{
    for (int i = 0; i < sizeof(pulse_tab) / sizeof(float); ++i)
        pulse_tab[i] = 95.52 / (8128.0 / (float)i + 100);

    for (int i = 0; i < sizeof(tnd_tab) / sizeof(float); ++i)
        tnd_tab[i] = 163.67 / (24329.0 / (float)i + 100);
}

static float calc_filter(FilterState* state, float B0, float B1, float A1,
                         float x)
{
    float y       = B0 * x + B1 * state->prev_x - A1 * state->prev_y;
    state->prev_x = x;
    state->prev_y = y;
    return y;
}

static float apply_filter(SoundFilter* sf, float x)
{
    static const float pi   = 3.14159265359;
    static const float gain = 1.0;

    float cutoff, c, a0i;

#if ENABLE_HIGH_FILTER_1
    // high pass filter
    cutoff = 90.0;
    c      = sf->sample_rate / pi / cutoff;
    a0i    = 1.0 / (1.0 + c);
    x      = calc_filter(&sf->high_90, c * a0i, -c * a0i, (1 - c) * a0i, x);
#endif

#if ENABLE_HIGH_FILTER_2
    // high pass filter
    cutoff = 440.0;
    c      = sf->sample_rate / pi / cutoff;
    a0i    = 1.0 / (1.0 + c);
    x      = calc_filter(&sf->high_440, c * a0i, -c * a0i, (1 - c) * a0i, x);
#endif

#if ENABLE_LOW_FILTER
    // low pass filter
    cutoff = 14000.0;
    c      = sf->sample_rate / pi / cutoff;
    a0i    = 1.0 / (1.0 + c);
    x      = calc_filter(&sf->low_14000, a0i, a0i, (1 - c) * a0i, x);
#endif

    return x * gain;
}

Apu* apu_build(struct System* sys)
{
    if (!SDL_WasInit(SDL_INIT_AUDIO))
        panic("you must init SDL Audio first");

    Apu* apu = calloc_or_fail(sizeof(Apu));
    apu->sys = sys;

    apu->pulse1.channel  = 0;
    apu->pulse2.channel  = 1;
    apu->noise.shift_reg = 1;
    apu->dmc.sys         = sys;

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq     = 44100;
    want.format   = AUDIO_F32;
    want.channels = 1;
    want.samples  = 2048;

    apu->dev = SDL_OpenAudioDevice(NULL, 0, &want, &apu->spec, 0);

    apu->filter.sample_rate   = apu->spec.freq;
    apu->sound_buffer_num_els = apu->spec.samples / 4;
    apu->sound_buffer =
        malloc_or_fail(apu->sound_buffer_num_els * sizeof(float));
    apu->sound_buffer_i = 0;
    apu->is_paused      = 1;
    return apu;
}

void apu_destroy(Apu* apu)
{
    SDL_CloseAudioDevice(apu->dev);
    free(apu->sound_buffer);
    free(apu);
}

static void pulse_write_control(Pulse* pulse, uint8_t value)
{
    pulse->duty_cycle                    = (value >> 6) & 3;
    pulse->length_counter_halt           = (value >> 5) & 1;
    pulse->constant_volume_envelope_flag = (value >> 4) & 1;
    pulse->volume_driver_period          = (value & 0x0F);

    pulse->envelope_start = 1;
}

static void pulse_write_sweep(Pulse* pulse, uint8_t value)
{
    pulse->sweep_enabled = (value >> 7) & 1;
    pulse->sweep_period  = (value >> 4) & 7;
    pulse->sweep_negate  = (value >> 3) & 1;
    pulse->sweep_shift   = value & 7;

    pulse->sweep_reload = 1;
}

static void pulse_write_timer_low(Pulse* pulse, uint8_t value)
{
    pulse->timer_period = (pulse->timer_period & 0xFF00) | (uint16_t)value;
}

static void pulse_write_timer_high(Pulse* pulse, uint8_t value)
{
    pulse->timer_period =
        (pulse->timer_period & 0x00FF) | ((uint16_t)(value & 7) << 8);
    pulse->length_value   = length_tab[value >> 3];
    pulse->envelope_start = 1;
    pulse->duty_value     = 0;
}

static void pulse_step_envelope(Pulse* pulse)
{
    if (pulse->envelope_start) {
        pulse->envelope_volume = 15;
        pulse->envelope_value  = pulse->volume_driver_period;
        pulse->envelope_start  = 0;
    } else if (pulse->envelope_value > 0) {
        pulse->envelope_value--;
    } else {
        if (pulse->envelope_volume > 0) {
            pulse->envelope_volume--;
        } else if (ENVELOPE_LOOP(pulse)) {
            pulse->envelope_volume = 15;
        }
        pulse->envelope_value = pulse->volume_driver_period;
    }
}

static void pulse_step_sweep(Pulse* pulse)
{
    int should_sweep = 0;
    if (pulse->sweep_reload) {
        if (pulse->sweep_enabled && (pulse->sweep_value == 0)) {
            should_sweep = 1;
        }
        pulse->sweep_value  = pulse->sweep_period + 1;
        pulse->sweep_reload = 0;
    } else if (pulse->sweep_value > 0) {
        pulse->sweep_value--;
    } else {
        if (pulse->sweep_enabled) {
            should_sweep = 1;
        }
        pulse->sweep_value = pulse->sweep_period + 1;
    }

    if (should_sweep) {
        uint8_t delta = pulse->timer_period >> pulse->sweep_shift;
        if (pulse->sweep_negate) {
            pulse->timer_period -= delta;
            if (pulse->channel == 0)
                pulse->timer_period--;
        } else {
            pulse->timer_period += delta;
        }
    }
}

static void pulse_step_length(Pulse* pulse)
{
    if (LENGTH_ENABLED(pulse) && pulse->length_value > 0)
        pulse->length_value--;
}

static void pulse_step_timer(Pulse* pulse)
{
    if (pulse->timer_value == 0) {
        pulse->timer_value = pulse->timer_period;
        pulse->duty_value  = (pulse->duty_value + 1) % 8;
    } else
        pulse->timer_value--;
}

static uint8_t pulse_output(Pulse* pulse)
{
    if (!pulse->enabled)
        return 0;

    if (pulse->length_value == 0)
        return 0;

    if (pulse->timer_period < 8 || pulse->timer_period > 0x7FF)
        return 0;

    if (duty_tab[pulse->duty_cycle][pulse->duty_value] == 0)
        return 0;

    if (pulse->constant_volume_envelope_flag)
        return pulse->volume_driver_period;
    return pulse->envelope_volume;
}

static void triangular_write_control(Triangular* t, uint8_t value)
{
    t->length_counter_halt = (value >> 7) & 1;
    t->counter_period      = value & 0x7F;
}

static void triangular_write_timer_low(Triangular* t, uint8_t value)
{
    t->timer_period = (t->timer_period & 0xFF00) | (uint16_t)value;
}

static void triangular_write_timer_high(Triangular* t, uint8_t value)
{
    t->length_value = length_tab[value >> 3];
    t->timer_period = (t->timer_period & 0x00FF) | ((uint16_t)(value & 7) << 8);
    t->timer_value  = t->timer_period;
    t->counter_reload = 1;
}

static void triangular_step_timer(Triangular* t)
{
    if (t->timer_value == 0) {
        t->timer_value = t->timer_period;
        if (t->length_value > 0 && t->counter_value > 0)
            t->duty_value = (t->duty_value + 1) % 32;
    } else {
        t->timer_value--;
    }
}

static void triangular_step_length(Triangular* t)
{
    if (!t->length_counter_halt && t->length_value > 0)
        t->length_value--;
}

static void triangular_step_counter(Triangular* t)
{
    if (t->counter_reload)
        t->counter_value = t->counter_period;
    else if (t->counter_value > 0)
        t->counter_value--;

    if (!t->length_counter_halt)
        t->counter_reload = 0;
}

static uint8_t triangular_output(Triangular* t)
{
    if (!t->enabled)
        return 0;

    if (t->timer_period < 3)
        return 0;

    if (t->length_value == 0)
        return 0;

    if (t->counter_value == 0)
        return 0;

    return triangle_tab[t->duty_value];
}

static void noise_write_control(Noise* n, uint8_t value)
{
    n->length_counter_halt           = (value >> 5) & 1;
    n->constant_volume_envelope_flag = (value >> 4) & 1;
    n->volume_driver_period          = (value & 0x0F);
    n->envelope_start                = 1;
}

static void noise_write_period(Noise* n, uint8_t value)
{
    n->mode         = (value & 0x80) == 0x80;
    n->timer_period = noise_tab[value & 0x0F];
}

static void noise_write_length(Noise* n, uint8_t value)
{
    n->length_value   = length_tab[value >> 3];
    n->envelope_start = 1;
}

static void noise_step_timer(Noise* n)
{
    if (n->timer_value == 0) {
        n->timer_value = n->timer_period;
        uint8_t shift  = n->mode ? 6 : 1;
        uint8_t b1     = n->shift_reg & 1;
        uint8_t b2     = (n->shift_reg >> shift) & 1;
        n->shift_reg >>= 1;
        n->shift_reg |= (b1 ^ b2) << 14;
    } else {
        n->timer_value--;
    }
}

static void noise_step_envelope(Noise* n)
{
    if (n->envelope_start) {
        n->envelope_volume = 15;
        n->envelope_value  = n->volume_driver_period;
        n->envelope_start  = 0;
    } else if (n->envelope_value > 0) {
        n->envelope_value--;
    } else {
        if (n->envelope_volume > 0) {
            n->envelope_volume--;
        } else if (ENVELOPE_LOOP(n)) {
            n->envelope_volume = 15;
        }
        n->envelope_value = n->volume_driver_period;
    }
}

static void noise_step_length(Noise* n)
{
    if (LENGTH_ENABLED(n) && n->length_value > 0)
        n->length_value--;
}

static uint8_t noise_output(Noise* n)
{
    if (!n->enabled)
        return 0;

    if (n->length_value == 0)
        return 0;

    if (n->shift_reg & 1)
        return 0;

    if (n->constant_volume_envelope_flag)
        return n->volume_driver_period;
    return n->envelope_volume;
}

static void dmc_write_control(DMC* dmc, uint8_t value)
{
    dmc->irq         = (value >> 7) & 1;
    dmc->loop        = (value >> 6) & 1;
    dmc->tick_period = dmc_tab[value & 0x0F];
}

static void dmc_write_value(DMC* dmc, uint8_t value)
{
    dmc->value = value & 0x7F;
}

static void dmc_write_address(DMC* dmc, uint8_t value)
{
    dmc->sample_addr = 0xC000 | ((uint16_t)value << 6);
}

static void dmc_write_length(DMC* dmc, uint8_t value)
{
    dmc->sample_length = ((uint16_t)value << 4) | 1;
}

static void dmc_restart(DMC* dmc)
{
    dmc->current_addr   = dmc->sample_addr;
    dmc->current_length = dmc->sample_length;
}

static void dmc_step_reader(DMC* dmc)
{
    if (dmc->current_length > 0 && dmc->bit_count == 0) {
        dmc->sys->cpu->stall += 4;
        dmc->shift_register =
            memory_read(dmc->sys->cpu->mem, dmc->current_addr);
        dmc->bit_count = 8;
        dmc->current_addr++;
        if (dmc->current_addr == 0)
            dmc->current_addr = 0x8000;
        dmc->current_length--;
        if (dmc->current_length == 0 && dmc->loop)
            dmc_restart(dmc);
    }
}

static void dmc_step_shifter(DMC* dmc)
{
    if (dmc->bit_count == 0)
        return;

    if (dmc->shift_register & 1) {
        if (dmc->value <= 125)
            dmc->value += 2;
    } else {
        if (dmc->value >= 2) {
            dmc->value -= 2;
        }
    }

    dmc->shift_register >>= 1;
    dmc->bit_count--;
}

static void dmc_step_timer(DMC* dmc)
{
    if (!dmc->enabled)
        return;

    dmc_step_reader(dmc);
    if (dmc->tick_value == 0) {
        dmc->tick_value = dmc->tick_period;
        dmc_step_shifter(dmc);
    } else {
        dmc->tick_value--;
    }
}

static uint8_t dmc_output(DMC* dmc) { return dmc->value; }

static void apu_step_envelope(Apu* apu)
{
    pulse_step_envelope(&apu->pulse1);
    pulse_step_envelope(&apu->pulse2);
    triangular_step_counter(&apu->triangular);
    noise_step_envelope(&apu->noise);
}

static void apu_step_length(Apu* apu)
{
    pulse_step_length(&apu->pulse1);
    pulse_step_length(&apu->pulse2);
    triangular_step_length(&apu->triangular);
    noise_step_length(&apu->noise);
}

static void apu_step_sweep(Apu* apu)
{
    pulse_step_sweep(&apu->pulse1);
    pulse_step_sweep(&apu->pulse2);
}

static void apu_write_control(Apu* apu, uint8_t value)
{
    apu->pulse1.enabled     = value & 1;
    apu->pulse2.enabled     = (value >> 1) & 1;
    apu->triangular.enabled = (value >> 2) & 1;
    apu->noise.enabled      = (value >> 3) & 1;
    apu->dmc.enabled        = (value >> 4) & 1;

    if (!apu->pulse1.enabled)
        apu->pulse1.length_value = 0;
    if (!apu->pulse2.enabled)
        apu->pulse2.length_value = 0;
    if (!apu->triangular.enabled)
        apu->triangular.length_value = 0;
    if (!apu->noise.enabled)
        apu->noise.length_value = 0;
    if (!apu->dmc.enabled)
        apu->dmc.current_length = 0;
    else if (apu->dmc.current_length == 0)
        dmc_restart(&apu->dmc);
}

static void apu_write_frame_counter(Apu* apu, uint8_t value)
{
    apu->frame_period_mode = (value >> 7) & 1;
    apu->frame_irq         = ((value >> 6) & 1) == 0;

    if (apu->frame_period_mode == 1) {
        apu_step_envelope(apu);
        apu_step_sweep(apu);
        apu_step_length(apu);
    }
}

void apu_write_register(Apu* apu, uint16_t addr, uint8_t value)
{
    switch (addr) {
        case 0x4000:
            pulse_write_control(&apu->pulse1, value);
            break;
        case 0x4001:
            pulse_write_sweep(&apu->pulse1, value);
            break;
        case 0x4002:
            pulse_write_timer_low(&apu->pulse1, value);
            break;
        case 0x4003:
            pulse_write_timer_high(&apu->pulse1, value);
            break;
        case 0x4004:
            pulse_write_control(&apu->pulse2, value);
            break;
        case 0x4005:
            pulse_write_sweep(&apu->pulse2, value);
            break;
        case 0x4006:
            pulse_write_timer_low(&apu->pulse2, value);
            break;
        case 0x4007:
            pulse_write_timer_high(&apu->pulse2, value);
            break;
        case 0x4008:
            triangular_write_control(&apu->triangular, value);
            break;
        case 0x4009:
        case 0x4010:
            dmc_write_control(&apu->dmc, value);
            break;
        case 0x4011:
            dmc_write_value(&apu->dmc, value);
            break;
        case 0x4012:
            dmc_write_address(&apu->dmc, value);
            break;
        case 0x4013:
            dmc_write_length(&apu->dmc, value);
            break;
        case 0x400A:
            triangular_write_timer_low(&apu->triangular, value);
            break;
        case 0x400B:
            triangular_write_timer_high(&apu->triangular, value);
            break;
        case 0x400C:
            noise_write_control(&apu->noise, value);
            break;
        case 0x400D:
        case 0x400E:
            noise_write_period(&apu->noise, value);
            break;
        case 0x400F:
            noise_write_length(&apu->noise, value);
            break;
        case 0x4015:
            apu_write_control(apu, value);
            break;
        case 0x4017:
            apu_write_frame_counter(apu, value);
            break;
        default:
            warning("apu_write_register(): unsupported addr 0x%04x [0x%02x]",
                    addr, value);
    }
}

static float apu_sample(Apu* apu)
{
    uint8_t p1 = 0, p2 = 0, t = 0, n = 0, d = 0;

#if ENABLE_PULSE1
    p1 = pulse_output(&apu->pulse1);
#endif

#if ENABLE_PULSE2
    p2 = pulse_output(&apu->pulse2);
#endif

#if ENABLE_TRIANG
    t = triangular_output(&apu->triangular);
#endif

#if ENABLE_NOISE
    n = noise_output(&apu->noise);
#endif

#if ENABLE_DMC
    d = dmc_output(&apu->dmc);
#endif

    float pulse_out = pulse_tab[p1 + p2];
    float tnd_out   = tnd_tab[3 * t + 2 * n + d];
    return pulse_out + tnd_out;
}

static void gen_sample(Apu* apu)
{
    float sample = apply_filter(&apu->filter, apu_sample(apu));

    apu->sound_buffer[apu->sound_buffer_i++] = sample;
    if (apu->sound_buffer_i >= apu->sound_buffer_num_els) {
        SDL_QueueAudio(apu->dev, apu->sound_buffer,
                       apu->sound_buffer_num_els * sizeof(float));
        apu->sound_buffer_i = 0;
    }
}

static void step_frame_counter(Apu* apu)
{
    // mode 0:    mode 1:       function
    // ---------  -----------  -----------------------------
    //  - - - f    - - - - -    IRQ (if bit 6 is clear)
    //  - l - l    l - l - -    Length counter and sweep
    //  e e e e    e e e e -    Envelope and linear counter

    if (apu->frame_period_mode == 0) {
        apu->frame_value = (apu->frame_value + 1) % 4;

        apu_step_envelope(apu);

        if (apu->frame_value == 1 || apu->frame_value == 3) {
            apu_step_sweep(apu);
            apu_step_length(apu);
        }

        if (apu->frame_value == 3 && apu->frame_irq) {
            cpu_trigger_irq(apu->sys->cpu);
        }

    } else {
        apu->frame_value = (apu->frame_value + 1) % 5;

        apu_step_envelope(apu);

        if (apu->frame_value == 1 || apu->frame_value == 3) {
            apu_step_sweep(apu);
            apu_step_length(apu);
        }
    }
}

void apu_step(Apu* apu)
{
    uint64_t prev_cycle = apu->cycles++;
    if (apu->cycles % 2 == 0) {
        pulse_step_timer(&apu->pulse1);
        pulse_step_timer(&apu->pulse2);
        noise_step_timer(&apu->noise);
        dmc_step_timer(&apu->dmc);
    }
    triangular_step_timer(&apu->triangular);

    uint64_t frame_counter_rate = apu->sys->cpu_freq / 240;
    if (prev_cycle % frame_counter_rate == 0)
        step_frame_counter(apu);

    if (apu->is_paused)
        return;

    static const float gap   = 1.0;
    uint64_t sample_gen_freq = apu->sys->cpu_freq / apu->spec.freq * gap;
    if (prev_cycle % sample_gen_freq == 0)
        gen_sample(apu);
}

void apu_queue_samples(Apu* apu, void* samples, uint32_t samples_size)
{
    SDL_QueueAudio(apu->dev, samples, samples_size);
}

void apu_unpause(Apu* apu)
{
    apu->is_paused = 0;
    SDL_PauseAudioDevice(apu->dev, 0);
}

void apu_pause(Apu* apu)
{
    apu->is_paused = 1;
    SDL_PauseAudioDevice(apu->dev, 1);
}
