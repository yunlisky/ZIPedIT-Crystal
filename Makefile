# ZIPedIT Crystal (C) — Makefile
#
# 编译器选择顺序：
#   1. 命令行显式指定       make CC=xxx
#   2. 项目内置便携工具链   tools/w64devkit/bin/gcc.exe
#   3. 系统 PATH 中的 gcc
#
# 内置工具链不入库（体积约 630 MB），需要时按 README 自行下载解压。

LOCAL_GCC := $(wildcard $(CURDIR)/tools/w64devkit/bin/gcc.exe)

ifeq ($(LOCAL_GCC),)
  ifeq ($(origin CC),default)
    CC := gcc
  endif
else
  ifeq ($(origin CC),default)
    CC := $(LOCAL_GCC)
  endif
  export PATH := $(CURDIR)/tools/w64devkit/bin;$(PATH)
endif

CFLAGS = -O2 -Wall -Wextra -std=c11 -municode -DUNICODE -D_UNICODE \
         -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -D_WIN32_IE=0x0A00
LIBS = -lgdi32 -luser32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid \
       -ladvapi32 -lbcrypt -lmsimg32
SRCS = src/main.c src/ui.c src/worker.c src/dialogs.c src/selftest.c src/util.c
TARGET = zipedit.exe

$(TARGET): $(SRCS) src/app.h
	@echo "  CC  = $(CC)"
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LIBS)

# 命令行自检：去重 / 分子目录 / 完整解压 / 多格式
selftest: $(TARGET)
	./$(TARGET) --selftest

clean:
	-del /Q $(TARGET) 2>nul

.PHONY: selftest clean
