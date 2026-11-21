<a id="top"></a>

# TinyAudioLab

[中文](#zh) | [English](#en)

<a id="zh"></a>
## 中文

TinyAudioLab 是一个轻量、无需繁琐样板代码的 **C 实时音频 DSP 与 MIDI 框架**，面向计算机音乐教学和快速原型开发。你只需通过简洁的回调函数（`setup`、`process_sample`、`on_note_on`）编写合成器或音频效果代码，即可在 **Windows、macOS、Linux 和 STM32 微控制器**上运行。

### 主要特性
* **类似 Processing 的简洁体验**：专注于 DSP 运算（`process_sample`）和 MIDI 事件（`on_note_on`、`on_note_off`、`on_cc`），无需处理音频驱动线程、环形缓冲区或操作系统专属 API。
* **交互式设备选择**：内置命令行设备枚举器，可在启动时选择 MIDI 控制器、音频输入设备（线路/麦克风）和音频输出设备。
* **单二进制文件的可移植性**：Windows（MSYS2 UCRT64）支持完整静态链接；macOS 使用第三方库静态链接的混合方案。
* **支持 GUI**：支持自定义 `int main(void)` 入口，可集成图形库（例如 **Raylib**），实现实时示波器或频谱分析仪。
* **支持嵌入式平台**：通过 `tinyaudiolab_stm32.c` 后端，将相同的学生 API 映射到 STM32 HAL SAI/I2S DMA 和 UART MIDI（31250 bps）。

---

### 架构与核心依赖

在桌面平台（Windows、macOS、Linux）上，**TinyAudioLab** 基于三个成熟的开源音频库构建了轻量的纯 C 抽象层：

| 组件 | 库 | 在 TinyAudioLab 中的作用 | 许可证 |
| :--- | :--- | :--- | :--- |
| **实时音频 I/O** | **[RtAudio](https://github.com/thestk/rtaudio)** | 跨平台低延迟音频流（WASAPI、CoreAudio、ALSA/JACK） | RtAudio（类似 MIT） |
| **实时 MIDI I/O** | **[RtMidi](https://github.com/thestk/rtmidi)** | 跨平台 MIDI 设备枚举与事件回调（WinMM、CoreMIDI、ALSA） | RtMidi（类似 MIT） |
| **音频文件 I/O** | **[libsndfile](https://github.com/libsndfile/libsndfile)** | 将 WAV、FLAC、OGG 和 MP3 文件读写为单声道浮点缓冲区 | LGPL-2.1 |

---

### 平台配置与构建说明

#### 1. Windows（MSYS2 UCRT64）
1. 安装 [MSYS2](https://www.msys2.org/)，并打开 **MSYS2 UCRT64** 终端。
2. 安装所需的静态工具链和库：
   ```bash
   pacman -Syu --noconfirm
   pacman -S --needed --noconfirm $(cat msys2-requirements.txt)
   ```
3. 将 `C:\msys64\ucrt64\bin` 添加到 Windows 系统的 `Path` 环境变量。
4. 在 **VS Code** 中打开项目文件夹，按 **`Ctrl + Shift + B`**（或按 **`F5`** 进行调试）构建 `build/tinyaudiolab_app.exe`。

#### 2. macOS（Apple Silicon 与 Intel）
1. 安装 Xcode Command Line Tools 和 [Homebrew](https://brew.sh/)：
   ```bash
   xcode-select --install
   brew install rtaudio rtmidi libsndfile pkg-config
   ```
2. 在项目根目录构建并运行（或在 VS Code 中按 **`Ctrl + Shift + B`** / **`F5`**）：
   ```bash
   make run
   ```
   *注意：第三方库会静态链接进二进制文件；系统原生的 `CoreAudio` 和 `CoreMIDI` 框架仍采用动态链接。*

#### 3. Linux（Ubuntu / Debian）
1. 使用 `apt` 安装开发包：
   ```bash
   sudo apt update
   sudo apt install -y build-essential pkg-config librtaudio-dev librtmidi-dev libsndfile1-dev gdb
   ```
2. 构建并运行：
   ```bash
   make run
   ```

#### 4. STM32 裸机（STM32CubeIDE / CubeMX）
1. 在 STM32CubeMX 中配置 **SAI/I2S**（48 kHz、16 位立体声、循环 DMA）和 **UART**（31250 bps、8N1、接收中断）。
2. 将 CubeMX 生成的 `Core/Src/main.c` 中的 `int main(void)` 重命名为 `void CubeMX_SystemInit(void)`，并移除其中的 `while(1)` 循环。
3. 将 `core/tinyaudiolab.h`、`core/tinyaudiolab_stm32.c` 和你的 `src/main.c` 添加到工程，并启用硬件 FPU 后编译。

---

### API 快速参考

```c
// 生命周期与 DSP 回调
void  setup(void);
void  cleanup(void);
float process_sample(float input);

// MIDI 与键盘回调
void  on_note_on(int note, float velocity);
void  on_note_off(int note);
void  on_cc(int cc, float value);
void  on_key_down(char key);

// 文件读写与音频数学工具
float *tal_load_audio(const char *filepath, int *out_length);
bool   tal_save_wav(const char *filepath, const float *data, int length);
float  tal_mtof(float midi_note);
float  tal_dbtoa(float db);
float  tal_clampf(float val, float min, float max);
float  tal_randf(void);
```

---

### 致谢与许可证

特别感谢 **Gary P. Scavone** 以及 **[RtAudio](https://github.com/thestk/rtaudio)** 和 **[RtMidi](https://github.com/thestk/rtmidi)** 的贡献者，同时感谢 **Erik de Castro Lopo** 和 **[libsndfile](https://github.com/libsndfile/libsndfile)** 社区提供基础设施，支持本教学框架实现跨平台音频与 MIDI 功能。

**TinyAudioLab** 本身采用 **MIT 许可证**发布。分发包含静态链接 `libsndfile` 的二进制文件时，请确保遵守 GNU 宽通用公共许可证（LGPL v2.1）的条款。

[返回顶部](#top) | [English](#en)

---

<a id="en"></a>
## English

A lightweight, zero-boilerplate **C Real-Time Audio DSP & MIDI** framework designed for computer music education and rapid prototyping. Write your synthesis or audio effect code once using clean callbacks (`setup`, `process_sample`, `on_note_on`), and run it seamlessly across **Windows, macOS, Linux, and STM32 microcontrollers**.

### Key Features
* **Processing-style Simplicity**: Focus purely on DSP math (`process_sample`) and MIDI events (`on_note_on`, `on_note_off`, `on_cc`) without wrestling with audio driver threads, ring buffers, or OS-specific APIs.
* **Interactive Device Selection**: Built-in CLI device enumerator for hot-selecting MIDI controllers, audio inputs (line/mic), and audio outputs at startup.
* **Single-Binary Portability**: Full static linking on Windows (MSYS2 UCRT64) and hybrid static third-party bundling on macOS.
* **GUI Ready**: Transparently supports custom `int main(void)` entry points for integrating graphics libraries (e.g., **Raylib**) for real-time oscilloscopes or spectrum analyzers.
* **Embedded Ready**: Drop-in `tinyaudiolab_stm32.c` backend maps the exact same student API to STM32 HAL SAI/I2S DMA and UART MIDI (31250 bps).

---

### Architecture & Core Dependencies

On desktop platforms (Windows, macOS, Linux), **TinyAudioLab** is built as a thin, pure-C abstraction layer on top of three battle-tested open-source audio libraries:

| Component | Library | Role in TinyAudioLab | License |
| :--- | :--- | :--- | :--- |
| **Real-Time Audio I/O** | **[RtAudio](https://github.com/thestk/rtaudio)** | Cross-platform low-latency audio streaming (WASAPI, CoreAudio, ALSA/JACK) | RtAudio (MIT-like) |
| **Real-Time MIDI I/O** | **[RtMidi](https://github.com/thestk/rtmidi)** | Cross-platform MIDI device enumeration and event callbacks (WinMM, CoreMIDI, ALSA) | RtMidi (MIT-like) |
| **Audio File I/O** | **[libsndfile](https://github.com/libsndfile/libsndfile)** | Reading and writing WAV, FLAC, OGG, and MP3 files into mono float buffers | LGPL-2.1 |

---

### Platform Setup & Build Instructions

#### 1. Windows (MSYS2 UCRT64)
1. Install [MSYS2](https://www.msys2.org/) and open the **MSYS2 UCRT64** terminal.
2. Install all required static toolchains and libraries:
   ```bash
   pacman -Syu --noconfirm
   pacman -S --needed --noconfirm $(cat msys2-requirements.txt)
   ```
3. Add `C:\msys64\ucrt64\bin` to your Windows System `Path` environment variable.
4. Open the folder in **VS Code** and press **`Ctrl + Shift + B`** (or **`F5`** to debug) to build `build/tinyaudiolab_app.exe`.

#### 2. macOS (Apple Silicon & Intel)
1. Install Xcode Command Line Tools and [Homebrew](https://brew.sh/):
   ```bash
   xcode-select --install
   brew install rtaudio rtmidi libsndfile pkg-config
   ```
2. Build and run from the project root (or press **`Ctrl + Shift + B`** / **`F5`** in VS Code):
   ```bash
   make run
   ```
   *(Note: Third-party libraries are statically linked into the binary while linking dynamically to native `CoreAudio` and `CoreMIDI` frameworks.)*

#### 3. Linux (Ubuntu / Debian)
1. Install development packages via `apt`:
   ```bash
   sudo apt update
   sudo apt install -y build-essential pkg-config librtaudio-dev librtmidi-dev libsndfile1-dev gdb
   ```
2. Build and run:
   ```bash
   make run
   ```

#### 4. STM32 Bare-Metal (STM32CubeIDE / CubeMX)
1. Configure **SAI/I2S** (48 kHz, 16-bit Stereo, Circular DMA) and **UART** (31250 bps, 8N1, RX Interrupt) in STM32CubeMX.
2. Rename CubeMX's generated `int main(void)` in `Core/Src/main.c` to `void CubeMX_SystemInit(void)` (remove its `while(1)` loop).
3. Add `core/tinyaudiolab.h`, `core/tinyaudiolab_stm32.c`, and your `src/main.c` to the project and compile with hardware FPU enabled.

---

### API Quick Reference

```c
// Lifecycle & DSP Callbacks
void  setup(void);
void  cleanup(void);
float process_sample(float input);

// MIDI & Keyboard Callbacks
void  on_note_on(int note, float velocity);
void  on_note_off(int note);
void  on_cc(int cc, float value);
void  on_key_down(char key);

// File I/O & Audio Math Utilities
float *tal_load_audio(const char *filepath, int *out_length);
bool   tal_save_wav(const char *filepath, const float *data, int length);
float  tal_mtof(float midi_note);
float  tal_dbtoa(float db);
float  tal_clampf(float val, float min, float max);
float  tal_randf(void);
```

---

### Acknowledgments & License

Special thanks to **Gary P. Scavone** and the contributors of **[RtAudio](https://github.com/thestk/rtaudio)** and **[RtMidi](https://github.com/thestk/rtmidi)**, as well as **Erik de Castro Lopo** and the **[libsndfile](https://github.com/libsndfile/libsndfile)** community for providing the foundational cross-platform audio and MIDI infrastructure that makes this educational framework possible.

**TinyAudioLab** itself is released under the **MIT License**. When distributing statically linked binaries that include `libsndfile`, please ensure compliance with the terms of the GNU Lesser General Public License (LGPL v2.1).

[Back to top](#top) | [中文](#zh)
