CC = gcc
CFLAGS = -m64 -s -Os -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -municode -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 -Ires -Isrc $(EXTRA_CFLAGS)
LIBS = -lkernel32 -luser32 -lshell32 -ladvapi32

.PHONY: all clean

all: PowerModeTray.exe spike_test.exe

res/resource.res: res/resource.rc app.manifest res/app.ico
	windres -I res -i res/resource.rc -O coff -o res/resource.res

PowerModeTray.exe: res/resource.res src/main.c src/power.c src/tray.c src/menu.c
	$(CC) $(CFLAGS) -mwindows src/main.c src/power.c src/tray.c src/menu.c res/resource.res -o PowerModeTray.exe $(LIBS)

spike_test.exe: res/resource.res src/power.c tests/spike_test.c
	$(CC) $(CFLAGS) -mconsole tests/spike_test.c src/power.c res/resource.res -o spike_test.exe -lkernel32 -luser32 -ladvapi32

clean:
	-$(RM) PowerModeTray.exe spike_test.exe res/resource.res
