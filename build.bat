@echo off
setlocal

rem Must run from an x64 Native Tools Command Prompt for VS
if not defined VSINSTALLDIR (
    echo ERROR: Visual Studio environment not found.
    echo        Run this from an "x64 Native Tools Command Prompt for VS".
    exit /b 1
)

rem vcvarsall exports VSCMD_ARG_TGT_ARCH; fall back to Platform if it is absent.
set _ARCH=%VSCMD_ARG_TGT_ARCH%
if not defined _ARCH set _ARCH=%Platform%
if /I not "%_ARCH%"=="x64" (
    echo ERROR: target architecture is "%_ARCH%", expected x64.
    echo        Run this from an "x64 Native Tools Command Prompt for VS".
    exit /b 1
)

set CFLAGS=/nologo /c /O1 /Os /MT /GS /guard:cf /W4 /WX /wd4100 /DUNICODE /D_UNICODE /DNDEBUG /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /I.\res /I.\src
set LFLAGS=/nologo /SUBSYSTEM:WINDOWS /GUARD:CF /OPT:REF /OPT:ICF kernel32.lib user32.lib shell32.lib advapi32.lib

echo [1/4] Compiling resource script...
rc.exe /nologo /i res /fo res\resource.res res\resource.rc
if %ERRORLEVEL% neq 0 goto :fail

echo [2/4] Compiling application translation units...
cl.exe %CFLAGS% src\main.c src\power.c src\tray.c src\menu.c
if %ERRORLEVEL% neq 0 goto :fail

echo [3/4] Linking PowerModeTray.exe...
link.exe %LFLAGS% main.obj power.obj tray.obj menu.obj res\resource.res /OUT:PowerModeTray.exe
if %ERRORLEVEL% neq 0 goto :fail

echo [4/4] Building headless CLI spike test harness...
cl.exe %CFLAGS% /D_CONSOLE tests\spike_test.c
if %ERRORLEVEL% neq 0 goto :fail

link.exe /nologo /SUBSYSTEM:CONSOLE /GUARD:CF /OPT:REF /OPT:ICF spike_test.obj power.obj res\resource.res /OUT:spike_test.exe kernel32.lib user32.lib advapi32.lib
if %ERRORLEVEL% neq 0 goto :fail

del *.obj res\resource.res 2>nul
echo Build successful: PowerModeTray.exe and spike_test.exe
exit /b 0

:fail
set _RC=%ERRORLEVEL%
del *.obj res\resource.res 2>nul
echo Build FAILED with exit code %_RC%.
exit /b %_RC%
