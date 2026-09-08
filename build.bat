@echo off
setlocal

set CFLAGS=/nologo /c /O1 /Os /MT /GS /W4 /WX /wd4100 /DUNICODE /D_UNICODE /DNDEBUG /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /I.\res /I.\src
set LFLAGS=/nologo /SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF kernel32.lib user32.lib shell32.lib advapi32.lib

echo [1/4] Compiling resource script...
rc.exe /nologo /i res /fo res\resource.res res\resource.rc
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [2/4] Compiling application translation units...
cl.exe %CFLAGS% src\main.c src\power.c src\tray.c src\menu.c
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [3/4] Linking PowerModeTray.exe...
link.exe %LFLAGS% main.obj power.obj tray.obj menu.obj res\resource.res /OUT:PowerModeTray.exe
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [4/4] Building headless CLI spike test harness...
cl.exe %CFLAGS% /D_CONSOLE tests\spike_test.c
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

link.exe /nologo /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF spike_test.obj power.obj res\resource.res /OUT:spike_test.exe kernel32.lib user32.lib advapi32.lib
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

del *.obj res\resource.res
echo Build successful: PowerModeTray.exe and spike_test.exe
