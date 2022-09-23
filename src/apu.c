#include "apu.h"
#include "logging.h"
#include "alloc.h"
#include "system.h"
#include "6502_cpu.h"

#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_SINE_WAVE 0

#define ENVELOPE_LOOP(wave)  ((wave)->length_counter_halt == 1)
#define LENGTH_ENABLED(wave) ((wave)->length_counter_halt == 0)

static float approx_sine(float x)
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

Apu* apu_build(struct System* sys)
{
    if (!SDL_WasInit(SDL_INIT_AUDIO))
        panic("you must init SDL Audio first");

    Apu* apu = calloc_or_fail(sizeof(Apu));
    apu->sys = sys;

    apu->pulse1.channel = 0;
    apu->pulse2.channel = 1;

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq     = 44100;
    want.format   = AUDIO_F32;
    want.channels = 1;
    want.samples  = 1024;

    apu->dev = SDL_OpenAudioDevice(NULL, 0, &want, &apu->spec, 0);

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

static void apu_step_envelope(Apu* apu)
{
    pulse_step_envelope(&apu->pulse1);
    pulse_step_envelope(&apu->pulse2);

    // TODO other waves
}

static void apu_step_length(Apu* apu)
{
    pulse_step_length(&apu->pulse1);
    pulse_step_length(&apu->pulse2);

    // TODO other waves
}

static void apu_step_sweep(Apu* apu)
{
    pulse_step_sweep(&apu->pulse1);
    pulse_step_sweep(&apu->pulse2);

    // TODO other waves
}

static void apu_write_control(Apu* apu, uint8_t value)
{
    apu->pulse1.enabled = value & 1;
    apu->pulse2.enabled = (value >> 1) & 1;

    // TODO other waves

    if (!apu->pulse1.enabled)
        apu->pulse1.length_value = 0;
    if (!apu->pulse2.enabled)
        apu->pulse2.length_value = 0;
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
    uint8_t p1 = pulse_output(&apu->pulse1);
    uint8_t p2 = pulse_output(&apu->pulse2);

    // TODO other waves

    float pulse_out = pulse_tab[p1 + p2];
    return pulse_out;
}

static void gen_sample(Apu* apu)
{
    float sample;

#if SAMPLE_SINE_WAVE
    static int   idx = 0, dir = 1;
    static float freq = 50;
    static float pi2  = 6.28318530718;
    static float t    = 0;
    sample            = 32000 * approx_sine(t);

    t += freq * pi2 / 44100.0;
    if (t >= pi2)
        t -= pi2;

    if (++idx == 256) {
        if (dir)
            freq += 10;
        else
            freq -= 10;
        if (freq > 2000)
            dir = 0;
        if (freq < 50)
            dir = 1;
        idx = 0;
    }
#else
    sample = apu_sample(apu);
#endif

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
    }
    // TODO other waves

    uint64_t frame_counter_rate = apu->sys->cpu_freq / 240;
    if (prev_cycle % frame_counter_rate == 0)
        step_frame_counter(apu);

    if (apu->is_paused)
        return;

    static const float gap   = 0.95;
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
