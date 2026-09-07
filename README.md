# Win11-Lightweight-Power-Tray

A lightweight, zero-dependency, open-source Win32 system tray utility written in pure C for Windows 11. It restores fast, 2-click mouse switching between user-configured Windows 11 Power Modes (Best Power Efficiency, Balanced, Best Performance) — replacing the 5-click navigation required by the modern Settings app.

## Background

## Status

**Phase 1 in progress**: core PowrProf dynamic-binding subsystem and a headless CLI spike test harness for CI validation. The tray icon, context menu, and hardware event handling land in later phases.

## Design constraints

- Pure C (C99/C11), Win32 API only — no C++, no third-party libraries.
- Zero external runtime dependencies: statically-linked CRT (`/MT`), dynamic linkage only against `kernel32`, `user32`, `shell32`, `advapi32`.
- `powrprof.dll` is loaded dynamically at runtime (`LoadLibraryExW`) — never linked against `powrprof.lib`.
- Targets Windows 11 (Build 22000+) exclusively; fails gracefully on unsupported systems.

## Building (Phase 1)

**MSVC** (from a Developer Command Prompt):

```bat
build.bat
```

**MinGW-w64**:

```sh
make
```

Both produce `spike_test.exe`, a headless CLI harness that verifies the PowrProf dynamic-binding subsystem against the running OS.
