# Phase 1: only the PowrProf subsystem + spike_test.exe CLI harness exist.
# PowerModeTray.exe (main.c/tray.c/menu.c) is added in Phase 2.

CC = gcc
CFLAGS = -s -Os -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -municode -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Ires -Isrc
LIBS = -lkernel32 -luser32 -ladvapi32

all: spike_test.exe

res/resource.res: res/resource.rc app.manifest
	windres -I res -i res/resource.rc -O coff -o res/resource.res

spike_test.exe: res/resource.res src/power.c tests/spike_test.c
	$(CC) $(CFLAGS) -mconsole tests/spike_test.c src/power.c res/resource.res -o spike_test.exe $(LIBS)

clean:
	-cmd.exe /C "del /f /q *.exe res\*.res 2>nul" || rm -f *.exe res/*.res
