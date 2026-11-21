# TinyAudioLab

A lightweight, zero-boilerplate **C11 Real-Time Audio DSP & MIDI** framework designed for computer music education and rapid prototyping. Write your synthesis or audio effect code once using clean callbacks (`setup`, `process_sample`, `on_note_on`), and run it seamlessly across **Windows, macOS, Linux, and STM32 microcontrollers**.

## Key Features
* **Processing-style Simplicity**: Focus purely on DSP math (`process_sample`) and MIDI events (`on_note_on`, `on_note_off`, `on_cc`) without wrestling with audio driver threads or ring buffers.
* **Single-Binary Portability**: Full static linking on Windows (MSYS2 UCRT64) and hybrid static third-party bundling on macOS.
* **GUI Ready**: Transparently supports custom `int main(void)` entry points for integrating **Raylib** visualizers or scopes.
* **Embedded Ready**: Drop-in `tinyaudiolab_stm32.c` backend maps the exact same student API to STM32 HAL SAI/I2S DMA and UART MIDI (31250 bps).

---

## Platform Setup & Build Instructions

### 1. Windows (MSYS2 UCRT64)
1. Install [MSYS2](https://www.msys2.org/) and open the **MSYS2 UCRT64** terminal.
2. Install all required static toolchains and libraries:
   ```bash
   pacman -Syu --noconfirm
   pacman -S --needed --noconfirm $(cat msys2-requirements.txt)
   ```
3. Add `C:\msys64\ucrt64\bin` to your Windows System `Path` environment variable.
4. Open the folder in **VS Code** and press **`Ctrl + Shift + B`** (or **`F5`** to debug) to build `build/tinyaudiolab_app.exe`.

### 2. macOS (Apple Silicon & Intel)
1. Install Xcode Command Line Tools and [Homebrew](https://brew.sh/):
   ```bash
   xcode-select --install
   brew install rtaudio rtmidi libsndfile raylib pkg-config
   ```
2. Build and run from the project root (or press **`Ctrl + Shift + B`** / **`F5`** in VS Code):
   ```bash
   make run
   ```
   *(Note: Third-party libraries are statically linked into the binary while linking dynamically to native `CoreAudio` and `CoreMIDI` frameworks.)*

### 3. Linux (Ubuntu / Debian)
1. Install development packages via `apt`:
   ```bash
   sudo apt update
   sudo apt install -y build-essential pkg-config librtaudio-dev librtmidi-dev libsndfile1-dev libraylib-dev gdb
   ```
2. Build and run:
   ```bash
   make run
   ```

### 4. STM32 Bare-Metal (STM32CubeIDE / CubeMX)
1. Configure **SAI/I2S** (48 kHz, 16-bit Stereo, Circular DMA) and **UART** (31250 bps, 8N1, RX Interrupt) in STM32CubeMX.
2. Rename CubeMX's generated `int main(void)` in `Core/Src/main.c` to `void CubeMX_SystemInit(void)` (remove its `while(1)` loop).
3. Add `core/tinyaudiolab.h`, `core/tinyaudiolab_stm32.c`, and your `src/main.c` to the project and compile with hardware FPU enabled.

---

## API Quick Reference

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
