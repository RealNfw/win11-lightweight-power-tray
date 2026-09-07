@echo off
setlocal

rem Phase 1: only the PowrProf subsystem + spike_test.exe CLI harness exist.
rem PowerModeTray.exe (main.c/tray.c/menu.c) is added in Phase 2.

set CFLAGS=/nologo /c /O1 /Os /MT /GS /W4 /WX /wd4100 /DUNICODE /D_UNICODE /DNDEBUG /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /I.\res /I.\src
set LFLAGS=/nologo /SUBSYSTEM:CONSOLE /OPT:REF /OPT:ICF kernel32.lib user32.lib advapi32.lib

echo [1/3] Compiling resource script...
rc.exe /nologo /i res /fo res\resource.res res\resource.rc
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [2/3] Building headless CLI spike test harness...
cl.exe %CFLAGS% /D_CONSOLE src\power.c tests\spike_test.c
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [3/3] Linking spike_test.exe...
link.exe %LFLAGS% power.obj spike_test.obj res\resource.res /OUT:spike_test.exe
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

del *.obj res\resource.res
echo Build successful: spike_test.exe
