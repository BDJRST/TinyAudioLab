#define TINYAUDIOLAB_INTERNAL
#include "tinyaudiolab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
#include <time.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <conio.h>
#else
    #include <termios.h>
    #include <unistd.h>
    #include <fcntl.h>
#endif

// 硬件非正规数防爆音宏 (FTZ / DAZ)
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <xmmintrin.h>
    #include <pmmintrin.h>
#endif

#include <rtaudio/rtaudio_c.h>
#include <rtmidi/rtmidi_c.h>
#include <sndfile.h>

// ============================================================================
// 1. 学生侧弱符号默认空实现
// ============================================================================
__attribute__((weak)) void  setup(void) {}
__attribute__((weak)) void  cleanup(void) {}
__attribute__((weak)) float process_sample(float input) { (void)input; return 0.0f; }
__attribute__((weak)) void  on_note_on(int note, float velocity) { (void)note; (void)velocity; }
__attribute__((weak)) void  on_note_off(int note) { (void)note; }
__attribute__((weak)) void  on_cc(int cc, float value) { (void)cc; (void)value; }
__attribute__((weak)) void  on_key_down(char key) { (void)key; }

// ============================================================================
// 2. 全局硬件句柄与运行状态
// ============================================================================
static rtaudio_t   g_audio      = NULL;
static RtMidiInPtr g_midiIn     = NULL;
static bool        g_midiOpened = false;
static atomic_bool g_started    = false;

// 托管加载缓冲区指针，退出时统一安全释放（防止学生代码内存泄漏）
#define MAX_LOADED_BUFFERS 16
static float *g_allocated_buffers[MAX_LOADED_BUFFERS] = {0};
static int    g_allocated_count = 0;

static void enable_hardware_ftz(void) {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#elif defined(__aarch64__)
    uint64_t fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1ULL << 24);
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#endif
}

static void sleep_ms(unsigned int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

#ifndef _WIN32
static struct termios g_orig_termios;
static bool g_termios_saved = false;

static void posix_enable_raw_mode(void) {
    if (isatty(STDIN_FILENO)) {
        tcgetattr(STDIN_FILENO, &g_orig_termios);
        g_termios_saved = true;
        struct termios raw = g_orig_termios;
        raw.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
}

static void posix_disable_raw_mode(void) {
    if (g_termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_termios_saved = false;
    }
}
#endif

// ============================================================================
// 3. 音频数学工具 (Utility Functions)
// ============================================================================
float tal_mtof(float midi_note) { return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f); }
float tal_dbtoa(float db)       { return powf(10.0f, db / 20.0f); }
float tal_randf(void)           { return ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f; }
float tal_clampf(float val, float min, float max) {
    return (val < min) ? min : (val > max ? max : val);
}

// ============================================================================
// 4. libsndfile 读写接口实现 (WAV / MP3 / FLAC / OGG)
// ============================================================================
float *tal_load_audio(const char *filepath, int *out_length) {
    if (out_length) *out_length = 0;

    SF_INFO sfinfo = {0};
    SNDFILE *file = sf_open(filepath, SFM_READ, &sfinfo);
    if (!file) {
        printf("[TinyAudioLab] Load Error: '%s' (%s)\n", filepath, sf_strerror(NULL));
        return NULL;
    }

    if (sfinfo.samplerate != TAL_SAMPLE_RATE) {
        printf("[TinyAudioLab] Warning: '%s' is %d Hz (engine is %d Hz)\n",
               filepath, sfinfo.samplerate, TAL_SAMPLE_RATE);
    }

    size_t total_items = (size_t)sfinfo.frames * (size_t)sfinfo.channels;
    float *interleaved = (float *)malloc(total_items * sizeof(float));
    float *mono        = (float *)malloc((size_t)sfinfo.frames * sizeof(float));
    if (!interleaved || !mono) {
        free(interleaved);
        free(mono);
        sf_close(file);
        return NULL;
    }

    sf_count_t frames = sf_readf_float(file, interleaved, sfinfo.frames);
    sf_close(file);

    // 统一混缩为单声道数组
    for (sf_count_t i = 0; i < frames; i++) {
        float sum = 0.0f;
        for (int ch = 0; ch < sfinfo.channels; ch++) {
            sum += interleaved[i * sfinfo.channels + ch];
        }
        mono[i] = sum / (float)sfinfo.channels;
    }
    free(interleaved);

    if (g_allocated_count < MAX_LOADED_BUFFERS) {
        g_allocated_buffers[g_allocated_count++] = mono;
    }

    if (out_length) *out_length = (int)frames;
    printf("[TinyAudioLab] Loaded: '%s' | Length: %d samples (%.2fs)\n",
           filepath, (int)frames, (float)frames / TAL_SAMPLE_RATE);
    return mono;
}

bool tal_save_wav(const char *filepath, const float *data, int length) {
    if (!data || length <= 0) {
        printf("[TinyAudioLab] Save Error: Invalid buffer or length <= 0\n");
        return false;
    }

    SF_INFO sfinfo = {
        .samplerate = TAL_SAMPLE_RATE,
        .channels   = 1,
        .format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16
    };

    SNDFILE *out = sf_open(filepath, SFM_WRITE, &sfinfo);
    if (!out) {
        printf("[TinyAudioLab] Save Error: Cannot open '%s' (%s)\n", filepath, sf_strerror(NULL));
        return false;
    }

    sf_count_t written = sf_writef_float(out, data, (sf_count_t)length);
    sf_close(out);

    printf("[TinyAudioLab] Saved WAV: '%s' | %lld samples (%.2fs)\n",
           filepath, (long long)written, (float)written / TAL_SAMPLE_RATE);
    return (written == (sf_count_t)length);
}

// ============================================================================
// 5. 设备菜单与后台线程回调
// ============================================================================
static int prompt_index(const char *prompt, int minVal, int maxVal, int defaultVal) {
    char line[64];
    while (1) {
        printf("%s", prompt);
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) return defaultVal;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') return defaultVal;
        char *end = NULL;
        long val = strtol(line, &end, 10);
        if (end != line && *end == '\0' && val >= minVal && val <= maxVal) return (int)val;
        printf("  [!] Enter %d ~ %d (or Enter for default).\n", minVal, maxVal);
    }
}

static void internal_midi_cb(double ts, const unsigned char *msg, size_t size, void *ud) {
    (void)ts; (void)ud;
    if (size < 3) return;
    uint8_t st = msg[0] & 0xF0, d1 = msg[1], d2 = msg[2];
    if (st == 0x90 && d2 > 0) on_note_on((int)d1, (float)d2 / 127.0f);
    else if (st == 0x80 || (st == 0x90 && d2 == 0)) on_note_off((int)d1);
    else if (st == 0xB0) on_cc((int)d1, (float)d2 / 127.0f);
}

static int internal_audio_cb(void *outBuf, void *inBuf, unsigned int nFrames,
                             double ts, rtaudio_stream_status_t st, void *ud) {
    (void)ts; (void)st; (void)ud;
    float *out = (float *)outBuf;
    const float *in = (const float *)inBuf;

    for (unsigned int i = 0; i < nFrames; i++) {
        float s = tal_clampf(process_sample(in ? in[i] : 0.0f), -1.0f, 1.0f);
        out[2 * i] = out[2 * i + 1] = s;
    }
    return 0;
}

// ============================================================================
// 6. 生命周期钩子: tal_start / tal_run / tal_stop
// ============================================================================
int tal_start(void) {
    if (atomic_load(&g_started)) return 0;
    enable_hardware_ftz();
    srand((unsigned int)time(NULL));

#ifdef _WIN32
    if (!GetConsoleWindow()) {
        AllocConsole();
        freopen("CONIN$", "r", stdin);
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    SetConsoleTitleA("TinyAudioLab Audio & MIDI Engine");
#endif

    printf("=== TinyAudioLab [Audio & MIDI] Engine Setup ===\n");

    // 1. MIDI 设备选择
    g_midiIn = rtmidi_in_create_default();
    unsigned int midiCount = rtmidi_get_port_count(g_midiIn);
    if (midiCount > 0) {
        char name[128];
        printf("  MIDI [-1] None\n");
        for (unsigned int i = 0; i < midiCount; i++) {
            int len = sizeof(name);
            rtmidi_get_port_name(g_midiIn, i, name, &len);
            printf("  MIDI [%u] %s\n", i, name);
        }
        int sel = prompt_index("Select MIDI (Enter = 0, -1 = None): ", -1, (int)midiCount - 1, 0);
        if (sel >= 0) {
            rtmidi_open_port(g_midiIn, (unsigned int)sel, "TinyAudioLab_In");
            rtmidi_in_set_callback(g_midiIn, internal_midi_cb, NULL);
            rtmidi_in_ignore_types(g_midiIn, true, true, true);
            g_midiOpened = true;
        }
    }

    // 2. RtAudio 设备枚举与选择（按平台自动匹配最佳低延迟 API）
#if defined(_WIN32)
    g_audio = rtaudio_create(RTAUDIO_API_WINDOWS_WASAPI);
#elif defined(__APPLE__)
    g_audio = rtaudio_create(RTAUDIO_API_MACOSX_CORE);
#else
    g_audio = rtaudio_create(RTAUDIO_API_UNSPECIFIED);
#endif

    int devCount = rtaudio_device_count(g_audio);
    unsigned int defIn  = rtaudio_get_default_input_device(g_audio);
    unsigned int defOut = rtaudio_get_default_output_device(g_audio);
    unsigned int inIds[64], outIds[64];
    int inCnt = 0, outCnt = 0, defOutIdx = 0;

    for (int i = 0; i < devCount; i++) {
        unsigned int id = rtaudio_get_device_id(g_audio, i);
        rtaudio_device_info_t info = rtaudio_get_device_info(g_audio, id);
        if (info.input_channels > 0 && inCnt < 64) {
            inIds[inCnt++] = id;
        }
        if (info.output_channels > 0 && outCnt < 64) {
            if (id == defOut) defOutIdx = outCnt;
            outIds[outCnt++] = id;
        }
    }
    if (outCnt == 0) {
        fprintf(stderr, "[TinyAudioLab] Error: No audio output device found.\n");
        return 1;
    }

    bool useIn = false;
    unsigned int chosenIn = 0;
    if (inCnt > 0) {
        printf("  In  [-1] None\n");
        for (int i = 0; i < inCnt; i++) {
            printf("  In  [%d] %s%s\n", i, rtaudio_get_device_info(g_audio, inIds[i]).name,
                   (inIds[i] == defIn) ? " [DEF]" : "");
        }
        char p[64];
        snprintf(p, sizeof(p), "Select Input (Enter = -1, 0 ~ %d): ", inCnt - 1);
        int selIn = prompt_index(p, -1, inCnt - 1, -1);
        if (selIn >= 0) { useIn = true; chosenIn = inIds[selIn]; }
    }

    for (int i = 0; i < outCnt; i++) {
        printf("  Out [%d] %s%s\n", i, rtaudio_get_device_info(g_audio, outIds[i]).name,
               (outIds[i] == defOut) ? " [DEF]" : "");
    }
    char pOut[64];
    snprintf(pOut, sizeof(pOut), "Select Output (Enter = %d): ", defOutIdx);
    unsigned int chosenOut = outIds[prompt_index(pOut, 0, outCnt - 1, defOutIdx)];

    // 3. 执行学生初始化并启动后台音频线程
    setup();

    rtaudio_stream_parameters_t outP = { .device_id = chosenOut, .num_channels = 2, .first_channel = 0 };
    rtaudio_stream_parameters_t inP  = { .device_id = chosenIn,  .num_channels = 1, .first_channel = 0 };
    unsigned int frames = TAL_BUFFER_SIZE;

    int err = rtaudio_open_stream(g_audio, &outP, useIn ? &inP : NULL, RTAUDIO_FORMAT_FLOAT32,
                                  TAL_SAMPLE_RATE, &frames, internal_audio_cb, NULL, NULL, NULL);
    if (err != 0 && useIn) {
        printf("[TinyAudioLab] Warning: Full-duplex failed, retrying output-only...\n");
        err = rtaudio_open_stream(g_audio, &outP, NULL, RTAUDIO_FORMAT_FLOAT32,
                                  TAL_SAMPLE_RATE, &frames, internal_audio_cb, NULL, NULL, NULL);
    }
    if (err != 0) {
        fprintf(stderr, "[TinyAudioLab] Error: Failed to open audio stream.\n");
        return 1;
    }

    rtaudio_start_stream(g_audio);
    atomic_store(&g_started, true);
    printf("=== Running (%d Hz) | [Space] C4 | [ESC] Quit ===\n", TAL_SAMPLE_RATE);
    return 0;
}

void tal_run(void) {
    if (!atomic_load(&g_started)) return;
#ifndef _WIN32
    posix_enable_raw_mode();
#endif
    while (1) {
        int ch = 0;
#ifdef _WIN32
        if (_kbhit()) {
            ch = _getch();
            if (ch == 0 || ch == 224) {
                (void)_getch(); // 丢弃方向键/功能键的第二字节扫描码
                continue;
            }
        } else {
            sleep_ms(10);
            continue;
        }
#else
        ch = getchar();
        if (ch == EOF) {
            sleep_ms(10);
            continue;
        }
#endif
        if (ch == 27) break; // ESC 退出
        if (ch == ' ') {
            on_note_on(60, 0.85f);
            sleep_ms(400);
            on_note_off(60);
        } else if (ch >= 32 && ch <= 126) {
            on_key_down((char)ch);
        }
    }
#ifndef _WIN32
    posix_disable_raw_mode();
#endif
}

void tal_stop(void) {
    if (!atomic_load(&g_started)) return;

    if (g_audio) {
        if (rtaudio_is_stream_running(g_audio)) rtaudio_stop_stream(g_audio);
        if (rtaudio_is_stream_open(g_audio))    rtaudio_close_stream(g_audio);
        rtaudio_destroy(g_audio);
        g_audio = NULL;
    }
    if (g_midiIn) {
        if (g_midiOpened) rtmidi_close_port(g_midiIn);
        rtmidi_in_free(g_midiIn);
        g_midiIn = NULL;
    }

    cleanup();

    for (int i = 0; i < g_allocated_count; i++) {
        free(g_allocated_buffers[i]);
        g_allocated_buffers[i] = NULL;
    }
    g_allocated_count = 0;

    atomic_store(&g_started, false);
    printf("\n=== TinyAudioLab Stopped ===\n");
}

// ============================================================================
// 弱符号默认 tal_main + 强符号物理入口 main
// ============================================================================
__attribute__((weak)) int tal_main(void) {
    tal_run();
    return 0;
}

#undef main
int main(void) {
    if (tal_start() != 0) return 1;
    int ret = tal_main();
    tal_stop();
    return ret;
}