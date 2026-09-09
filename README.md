# Win11-Lightweight-Power-Tray

[![CI](https://github.com/RealNfw/win11-lightweight-power-tray/actions/workflows/ci.yml/badge.svg)](https://github.com/RealNfw/win11-lightweight-power-tray/actions/workflows/ci.yml)

Fast, 2-click switching for Windows 11 Power Modes right from your system tray.

Windows 11 moved power mode toggles deep into the Settings app (`ms-settings:powersleep`). This utility restores instant switching between Best Power Efficiency, Balanced, and Best Performance without opening a Settings window.

## Features

- **Fast Switching** — Left- or right-click the tray icon to change AC (Plugged In) and DC (On Battery) modes independently.
- **Zero Background Polling** — True event-driven design with 0% CPU consumption while idle.
- **Standalone Binary** — Small, fully self-contained single executable.. No installers, no runtimes, and no Visual C++ Redistributables required.
- **No Admin Rights** — Runs entirely within standard user permissions (`asInvoker`).
- **Hardware-Aware** — Automatically greys out battery settings on desktop PCs without batteries.

## Usage

1. Download and run `PowerModeTray.exe`. A lightning bolt icon will appear in the system notification area.
2. Click the icon (left or right click) to open the power menu and select a mode.
3. Hover over the icon to see the currently active profile and power source.

Launching the `.exe` while it is already running summons the existing tray menu instead of creating a duplicate instance.

## Run at Startup

To launch automatically when logging into Windows:

1. Press <kbd>Win</kbd> + <kbd>R</kbd>, type `shell:startup`, and press <kbd>Enter</kbd>.
2. Place a shortcut to `PowerModeTray.exe` in the folder.

## Building from Source

Requires Windows 11 (x64). Both build options produce `PowerModeTray.exe` and a headless test harness `spike_test.exe`.

### MSVC (Recommended)

Open an **x64 Native Tools Command Prompt for VS** and run:

```bat
build.bat
```

### MinGW-w64

Run from any terminal with GCC in your `PATH`:

```sh
make
```

## License

MIT
