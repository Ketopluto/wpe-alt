# WPE-Alt

**Open-source Wallpaper Engine alternative** — feature-complete desktop wallpaper manager for Windows.

## Features

- **Video wallpapers** — looped video playback (MP4, MKV, WebM, AVI, MOV, and more)
- **Image wallpapers** — static images (JPG, PNG, BMP, WebP)
- **GIF / APNG wallpapers** — animated image support
- **Playlist system** — sequential, shuffle, or single-loop modes with timed auto-rotation
- **Fill modes** — Stretch, Fill (crop), Fit (letterbox), Center
- **Volume & mute** — audio control for video wallpapers
- **Wallpaper gallery** — browse, add, and manage wallpapers with thumbnails
- **Settings dialog** — configure everything from a tabbed UI
- **Auto-start** — launch on Windows boot
- **Pause on fullscreen** — automatically pause when a fullscreen app/game is detected
- **Pause on battery** — save power when unplugged
- **Single instance** — prevents multiple copies from running
- **System tray** — full control from the tray icon (pause, next/prev, mute, settings, gallery)
- **Desktop integration** — renders behind icons via Windows WorkerW

## Tech Stack

- **Qt 6** (`Core`, `Gui`, `Widgets`) — app runtime + UI
- **FFmpeg** (`avformat`, `avcodec`, `swscale`, `avutil`) — broad video decode support
- **C++17** — modern language features

## Build

### Prerequisites

- CMake `>= 3.20`
- C++17 compiler (MSVC, GCC, Clang)
- Qt6 development packages
- FFmpeg development packages

### Windows with vcpkg (recommended)

```powershell
vcpkg\vcpkg.exe install qtbase:x64-windows ffmpeg:x64-windows
cmake -S . -B build
cmake --build build --config Release
```

## Run

```powershell
build\Release\wpe-lite.exe
```

With custom config:

```powershell
build\Release\wpe-lite.exe --config C:\path\to\wallpaper.ini
```

## Configuration

`wallpaper.ini` is read from the executable folder by default.

```ini
[engine]
fps=30
fill_mode=fill          # stretch | fill | fit | center
auto_start=false

[media]
items=C:\media\wallpaper.mp4;C:\media\bg.png
start_index=0

[playlist]
mode=sequential         # sequential | shuffle | single
interval_sec=0          # 0 = no auto-rotation

[audio]
volume=100              # 0-100
mute=false

[behavior]
pause_fullscreen=true   # pause when fullscreen app detected
pause_battery=false     # pause when on battery power
```

## Architecture

```
src/
├── core/
│   ├── wallpaper_engine.h/cpp   # render loop + window management
│   └── playlist.h               # media queue + rotation timer
├── renderers/
│   ├── wallpaper_renderer.h     # abstract base (render, resize, fill, audio)
│   ├── video_renderer.h/cpp     # FFmpeg decode thread + QBackingStore blit
│   ├── image_renderer.h         # static image with fill modes
│   ├── gif_renderer.h           # animated GIF/APNG via QMovie
│   └── color_renderer.h         # animated gradient fallback
├── gui/
│   ├── tray_icon.h              # system tray with full controls
│   ├── settings_dialog.h        # tabbed settings UI
│   └── wallpaper_gallery.h      # thumbnail grid browser
├── platform/
│   ├── platform_utils.h         # abstract desktop integration
│   ├── windows_utils.cpp        # WorkerW hosting + input passthrough
│   ├── stub_utils.cpp           # non-Windows fallback
│   └── system_integration.h     # auto-start, fullscreen/battery/lock detection
└── main.cpp                     # entry point, wires everything together
```

## Performance Notes

- Rendering FPS is capped by `[engine].fps`
- Video decode runs on a dedicated thread
- Frames are resized once in FFmpeg scaling path (not per-paint)
- Fullscreen/battery polling runs every 2 seconds (negligible overhead)
- Thumbnails are scaled during load to avoid full-resolution memory usage

## Roadmap

- Hardware decode integration (DXVA2/D3D11VA)
- Web/HTML wallpapers
- Shader-based wallpapers (GLSL)
- Wallpaper Engine `.pkg` file import
- Native Linux/macOS desktop integration

## License

MIT
