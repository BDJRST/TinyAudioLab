#define TINYAUDIOLAB_INTERNAL
#include "tinyaudiolab.h"

#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 外部 CubeMX 外设句柄（按实际工程修改）
extern SAI_HandleTypeDef  hsai_BlockA1; // 音频输出 (TX)
extern SAI_HandleTypeDef  hsai_BlockB1; // 音频输入 (RX)
extern UART_HandleTypeDef huart2;       // MIDI 输入 (31250 bps)

// ============================================================================
// 1. 学生侧弱符号默认空实现
// ============================================================================
__attribute__((weak)) void  setup(void) {}
__attribute__((weak)) void  cleanup(void) {}
__attribute__((weak)) float process_sample(float input) { return input; }
__attribute__((weak)) void  on_note_on(int note, float velocity) { (void)note; (void)velocity; }
__attribute__((weak)) void  on_note_off(int note) { (void)note; }
__attribute__((weak)) void  on_cc(int cc, float value) { (void)cc; (void)value; }
__attribute__((weak)) void  on_key_down(char key) { (void)key; }

// ============================================================================
// 2. 音频 DMA 双缓冲 (Ping-Pong Buffer)
// 注：STM32H7 请将缓冲区放入 Non-Cacheable 的 .RAM_D2 段
// ============================================================================
#define DMA_TOTAL_SAMPLES (TAL_BUFFER_SIZE * 2 * 2)

static int16_t audio_rx_buf[DMA_TOTAL_SAMPLES] __attribute__((aligned(32)));
static int16_t audio_tx_buf[DMA_TOTAL_SAMPLES] __attribute__((aligned(32)));
static volatile bool g_audio_running = false;

typedef struct {
    uint32_t deadline;
    bool active;
} makenote_timer_t;

static makenote_timer_t g_makenote_timers[128];

void makenote(int note, float velocity, uint32_t duration_ms) {
    if (note < 0 || note > 127) {
        printf("[TinyAudioLab] makenote: note must be in range 0~127\n");
        return;
    }
    if (duration_ms > INT32_MAX) {
        printf("[TinyAudioLab] makenote: duration must not exceed %d ms\n", INT32_MAX);
        return;
    }

    g_makenote_timers[note].deadline = HAL_GetTick() + duration_ms;
    g_makenote_timers[note].active = true;
    on_note_on(note, velocity);
}

static void process_makenote_timers(void) {
    uint32_t now = HAL_GetTick();
    for (int note = 0; note < 128; note++) {
        if (g_makenote_timers[note].active &&
            (int32_t)(now - g_makenote_timers[note].deadline) >= 0) {
            g_makenote_timers[note].active = false;
            on_note_off(note);
        }
    }
}

static void stop_makenote_timers(void) {
    for (int note = 0; note < 128; note++) {
        if (g_makenote_timers[note].active) {
            g_makenote_timers[note].active = false;
            on_note_off(note);
        }
    }
}

static void audio_process_half_block(int16_t *rx_ptr, int16_t *tx_ptr) {
    for (int i = 0; i < TAL_BUFFER_SIZE; i++) {
        float in_l = (float)rx_ptr[i * 2] / 32768.0f;
        float out  = g_audio_running ? process_sample(in_l) : 0.0f;
        out = tal_clampf(out, -1.0f, 1.0f);

        int16_t pcm_out = (int16_t)(out * 32767.0f);
        tx_ptr[i * 2]     = pcm_out;
        tx_ptr[i * 2 + 1] = pcm_out;
    }
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
    if (hsai == &hsai_BlockB1) {
        audio_process_half_block(&audio_rx_buf[0], &audio_tx_buf[0]);
    }
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {
    if (hsai == &hsai_BlockB1) {
        audio_process_half_block(&audio_rx_buf[TAL_BUFFER_SIZE * 2], &audio_tx_buf[TAL_BUFFER_SIZE * 2]);
    }
}

// ============================================================================
// 3. UART MIDI 接收环形队列与字节流状态机（支持 Running Status）
// ============================================================================
#define MIDI_RING_SIZE 256
static volatile uint8_t  midi_ring[MIDI_RING_SIZE];
static volatile uint16_t midi_head = 0;
static volatile uint16_t midi_tail = 0;
static uint8_t           midi_rx_byte = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart2) {
        uint16_t next = (midi_head + 1) & (MIDI_RING_SIZE - 1);
        if (next != midi_tail) {
            midi_ring[midi_head] = midi_rx_byte;
            midi_head = next;
        }
        HAL_UART_Receive_IT(&huart2, &midi_rx_byte, 1);
    }
}

static void midi_poll_and_dispatch(void) {
    static uint8_t status = 0;
    static uint8_t data[2];
    static uint8_t data_idx = 0;

    while (midi_tail != midi_head) {
        uint8_t byte = midi_ring[midi_tail];
        midi_tail = (midi_tail + 1) & (MIDI_RING_SIZE - 1);

        if (byte >= 0xF8) continue;

        if (byte & 0x80) {
            status = (byte >= 0xF0) ? 0 : byte;
            data_idx = 0;
        } else if (status != 0) {
            data[data_idx++] = byte;
            uint8_t cmd = status & 0xF0;
            uint8_t expected = (cmd == 0xC0 || cmd == 0xD0) ? 1 : 2;

            if (data_idx >= expected) {
                if (cmd == 0x90) {
                    if (data[1] > 0) on_note_on((int)data[0], (float)data[1] / 127.0f);
                    else             on_note_off((int)data[0]);
                } else if (cmd == 0x80) {
                    on_note_off((int)data[0]);
                } else if (cmd == 0xB0) {
                    on_cc((int)data[0], (float)data[1] / 127.0f);
                }
                data_idx = 0;
            }
        }
    }
}

// ============================================================================
// 4. 文件读写空桩（极简嵌入式模式）
// ============================================================================
float *tal_load_audio(const char *filepath, int *out_length) {
    (void)filepath;
    if (out_length) *out_length = 0;
    return NULL;
}

bool tal_save_wav(const char *filepath, const float *data, int length) {
    (void)filepath; (void)data; (void)length;
    return false;
}

// ============================================================================
// 5. 音频数学工具 (Utility Functions)
// ============================================================================
float tal_mtof(float midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f);
}

float tal_dbtoa(float db) {
    return powf(10.0f, db / 20.0f);
}

float tal_clampf(float val, float min, float max) {
    return (val < min) ? min : ((val > max) ? max : val);
}

float tal_randf(void) {
    return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
}

// ============================================================================
// 6. 引擎生命周期与主入口接管
// ============================================================================
int tal_start(void) {
    memset(audio_rx_buf, 0, sizeof(audio_rx_buf));
    memset(audio_tx_buf, 0, sizeof(audio_tx_buf));
    memset(g_makenote_timers, 0, sizeof(g_makenote_timers));

    setup();

    HAL_UART_Receive_IT(&huart2, &midi_rx_byte, 1);

    g_audio_running = true;
    HAL_SAI_Transmit_DMA(&hsai_BlockA1, (uint8_t *)audio_tx_buf, DMA_TOTAL_SAMPLES);
    HAL_SAI_Receive_DMA(&hsai_BlockB1,  (uint8_t *)audio_rx_buf, DMA_TOTAL_SAMPLES);
    return 0;
}

void tal_run(void) {
    while (1) {
        process_makenote_timers();
        midi_poll_and_dispatch();
    }
}

void tal_stop(void) {
    stop_makenote_timers();
    g_audio_running = false;
    HAL_SAI_DMAStop(&hsai_BlockA1);
    HAL_SAI_DMAStop(&hsai_BlockB1);
    cleanup();
}

__attribute__((weak)) int tal_main(void) {
    tal_run();
    return 0;
}

extern void CubeMX_SystemInit(void);

#undef main
int main(void) {
    CubeMX_SystemInit();
    if (tal_start() == 0) {
        tal_main();
    }
    tal_stop();
    while (1) {}
}
