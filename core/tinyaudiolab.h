#ifndef TINYAUDIOLAB_H
#define TINYAUDIOLAB_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// 平台自动检测宏
// ============================================================================
#if defined(__arm__) || defined(__ARM_ARCH) || defined(USE_HAL_DRIVER)
    #define TAL_PLATFORM_STM32 1
#elif defined(_WIN32)
    #define TAL_PLATFORM_WIN   1
#elif defined(__APPLE__)
    #define TAL_PLATFORM_MAC   1
#else
    #define TAL_PLATFORM_LINUX 1
#endif

// ============================================================================
// 基础音频引擎常量
// ============================================================================
#define TAL_SAMPLE_RATE 48000
#define TAL_BUFFER_SIZE 256

// 接管用户代码中的标准 main()，使 Raylib/GUI 或默认控制台循环均可正常运行
#ifndef TINYAUDIOLAB_INTERNAL
    #define main tal_main
#endif

// ============================================================================
// 1. 学生侧生命周期与交互回调（弱符号默认空实现，按需编写即可）
// ============================================================================
void  setup(void);                           // 硬件/音频就绪后、音频流启动前执行一次
void  cleanup(void);                         // 音频流关闭后执行一次（可选）
float process_sample(float input);           // 逐采样点 DSP 处理（48kHz 实时音频线程）

void  on_note_on(int note, float velocity);  // MIDI 音符按下 (note: 0~127, vel: 0.0~1.0)
void  on_note_off(int note);                 // MIDI 音符松开
void  on_cc(int cc, float value);            // MIDI 控制器旋钮/推子 (val: 0.0~1.0)
void  on_key_down(char key);                 // 键盘按键触发（PC 主线程运行，STM32 下忽略）

// ============================================================================
// 2. 引擎控制钩子
// ============================================================================
int  tal_start(void);  // 初始化设备、调用 setup() 并启动实时音频/MIDI
void tal_run(void);    // 主循环（PC 端监听键盘；STM32 端解析 UART MIDI 队列）
void tal_stop(void);   // 停止音频流并释放资源
int  tal_main(void);   // 默认主入口（学生可手写 int main(void) 接管 GUI 主线程）

// ============================================================================
// 3. 音频文件读写接口（PC 端基于 libsndfile；STM32 极简版返回空桩）
// ============================================================================
float *tal_load_audio(const char *filepath, int *out_length);
bool   tal_save_wav(const char *filepath, const float *data, int length);

// ============================================================================
// 4. 常用音频数学工具 (Utility Functions)
// ============================================================================
float tal_mtof(float midi_note);
float tal_dbtoa(float db);
float tal_clampf(float val, float min, float max);
float tal_randf(void);

#endif // TINYAUDIOLAB_H
