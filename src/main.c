#include "tinyaudiolab.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

static volatile float g_freq  = 440.0f;
static volatile float g_amp   = 0.0f;
static float          g_phase = 0.0f;

void setup(void) {
    g_freq = tal_mtof(60.0f); // 默认 C4
}

void on_note_on(int note, float velocity) {
    g_freq = tal_mtof((float)note);
    g_amp  = velocity * 0.5f;
}

void on_note_off(int note) {
    (void)note;
    g_amp = 0.0f;
}

float process_sample(float input) {
    (void)input;
    float out = sinf(2.0f * M_PI * g_phase) * g_amp;
    g_phase += g_freq / (float)TAL_SAMPLE_RATE;
    if (g_phase >= 1.0f) g_phase -= 1.0f;
    return out;
}
