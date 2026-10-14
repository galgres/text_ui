# text_ui

A Windows-only C++ terminal user interface built with the WinAPI console APIs
and the C++ standard library. It uses no UI framework or third-party library.

## Build with Visual Studio

Open `text_ui.sln`, select a Debug or Release configuration and an x64 or x86
platform, and choose **Build > Build Solution**.

## Build with CMake

From a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

Run `build\Release\text_ui.exe` (multi-configuration generators) or
`build\text_ui.exe` (single-configuration generators).

## Controls

- `Tab`, Left Arrow, or Right Arrow switches the focused button.
- `Enter` or Space activates the focused button.
- `Escape` closes the application.
- **Change color** toggles the window between red and purple.
- **Exit** closes the application.
