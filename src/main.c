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

void on_key_down(char key) {
    if (key == 'a') {
        g_freq = tal_mtof(60.0f); // C4
        makenote(60, 0.5f, 500); // 非阻塞演奏 C4，持续 500ms
    } else if (key == 's') {
        g_freq = tal_mtof(62.0f); // D4
        makenote(62, 0.5f, 500); // 非阻塞演奏 D4，持续 500ms
    } else if (key == 'd') {
        g_freq = tal_mtof(64.0f); // E4
        makenote(64, 0.5f, 500); // 非阻塞演奏 E4，持续 500ms
    } else if (key == 'f') {
        g_freq = tal_mtof(65.0f); // F4
        makenote(65, 0.5f, 500); // 非阻塞演奏 F4，持续 500ms
    } else if (key == 'g') {
        g_freq = tal_mtof(67.0f); // G4
        makenote(67, 0.5f, 500); // 非阻塞演奏 G4，持续 500ms
    } else if (key == 'h') {
        g_freq = tal_mtof(69.0f); // A4
        makenote(69, 0.5f, 500); // 非阻塞演奏 A4，持续 500ms
    } else if (key == 'j') {
        g_freq = tal_mtof(71.0f); // B4
        makenote(71, 0.5f, 500); // 非阻塞演奏 B4，持续 500ms
    } else if (key == 'k') {
        g_freq = tal_mtof(72.0f); // C5
        makenote(72, 0.5f, 500); // 非阻塞演奏 C5，持续 500ms
    }
}

float process_sample(float input) {
    (void)input;
    float out = sinf(2.0f * M_PI * g_phase) * g_amp;
    g_phase += g_freq / (float)TAL_SAMPLE_RATE;
    if (g_phase >= 1.0f) g_phase -= 1.0f;
    return out;
}
