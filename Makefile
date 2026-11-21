CC      := gcc
CFLAGS  := -std=c11 -O2 -ffp-contract=fast -fno-math-errno -Wall -Wextra -Wuninitialized -g
INC     := -Icore

SRCS    := src/main.c core/tinyaudiolab.c
BUILD   := build

ifeq ($(OS),Windows_NT)
    # ---------------- Windows (MSYS2 UCRT64 全静态链接) ----------------
    TARGET  := $(BUILD)/tinyaudiolab_app.exe
    CFLAGS  += -msse2 -mfpmath=sse
    LDFLAGS := -static
    LIBS    := -lrtaudio -lrtmidi -lsndfile -lstdc++ \
               -lFLAC -lvorbisenc -lvorbis -logg -lopus -lmpg123 -lmp3lame -lshlwapi \
               -lwinmm -lole32 -ldsound -lksuser -lmfplat -lmfuuid -lwmcodecdspuuid -lm
               # 如需启用 Raylib，在上方追加：-lraylib -lopengl32 -lgdi32
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Darwin)
        # ---------------- macOS (混合链接：第三方静态 + 系统框架动态) ----------------
        CC      := clang
        TARGET  := $(BUILD)/tinyaudiolab_app
        BREW    := $(shell brew --prefix)
        INC     += -I$(BREW)/include
        LIBS    := $(BREW)/opt/rtaudio/lib/librtaudio.a \
                   $(BREW)/opt/rtmidi/lib/librtmidi.a \
                   $(BREW)/opt/libsndfile/lib/libsndfile.a \
                   $(BREW)/opt/flac/lib/libFLAC.a \
                   $(BREW)/opt/libvorbis/lib/libvorbisenc.a \
                   $(BREW)/opt/libvorbis/lib/libvorbis.a \
                   $(BREW)/opt/libogg/lib/libogg.a \
                   $(BREW)/opt/opus/lib/libopus.a \
                   $(BREW)/opt/mpg123/lib/libmpg123.a \
                   $(BREW)/opt/lame/lib/libmp3lame.a \
                   -lc++ -framework CoreAudio -framework CoreMIDI -framework CoreFoundation
                   # 如需启用 Raylib，在上方追加：$(BREW)/opt/raylib/lib/libraylib.a -framework IOKit -framework Cocoa -framework OpenGL
    else
        # ---------------- Linux (Ubuntu / Debian / Arch) ----------------
        TARGET  := $(BUILD)/tinyaudiolab_app
        CFLAGS  += -march=native
        INC     += $(shell pkg-config --cflags rtaudio rtmidi sndfile)
        LIBS    := $(shell pkg-config --libs rtaudio rtmidi sndfile) -lm -lpthread
        # 如需启用 Raylib，在 pkg-config 参数后追加 raylib
    endif
endif

all: $(TARGET)

$(TARGET): $(SRCS)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(INC) $(SRCS) -o $(TARGET) $(LDFLAGS) $(LIBS)

run: all
	./$(TARGET)

clean:
	rm -rf $(BUILD)

.PHONY: all run clean