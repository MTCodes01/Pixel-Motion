# Pixel Motion

A high-performance, system-level Windows application that transforms your desktop background into an intelligent, resource-aware visual canvas.

## Features

- 🎬 **Video & Image Wallpapers** - Display your own videos and images as animated wallpapers
- 🖥️ **Multi-Monitor Support** - Independent wallpapers per monitor with custom scaling
- ⚡ **Hardware Accelerated** - FFmpeg D3D11VA + DirectX 11 rendering
- 🎮 **Game Mode** - Automatically pauses when fullscreen games are detected
- 🔋 **Battery-Aware** - Reduces frame rate or pauses on battery power
- 🎯 **Zero Performance Impact** - Near-zero CPU/GPU usage when idle

## Requirements

- Windows 11 (64-bit)
- DirectX 11 compatible GPU
- Visual Studio 2022 with C++ desktop development workload

## Building from Source

### Prerequisites

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) with "Desktop development with C++" workload
2. Install [vcpkg](https://github.com/microsoft/vcpkg):
   ```powershell
   git clone https://github.com/microsoft/vcpkg.git
   cd vcpkg
   .\bootstrap-vcpkg.bat
   ```
3. Set the `VCPKG_ROOT` environment variable:
   ```powershell
   [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\path\to\vcpkg', 'User')
   ```

### Build Steps

```powershell
# Clone the repository
git clone https://github.com/yourusername/Pixel-Motion.git
cd Pixel-Motion

# Configure with CMake
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

# Build
cmake --build build --config Release

# Run
.\build\bin\Release\PixelMotion.exe
```

## Usage

1. Launch Pixel Motion - it will appear in the system tray
2. Right-click the tray icon → **Settings**
3. Select your monitor and choose a video/image file
4. Configure playback options (loop, scaling, volume)
5. Enable Game Mode and Battery-Aware Mode as desired

## Architecture

```
┌─────────────────────────────────────────┐
│         Application Core                │
│  (Config, Tray UI, Monitor Manager)     │
└─────────────────────────────────────────┘
           │              │
    ┌──────┴──────┐  ┌───┴────────────┐
    │  Rendering  │  │ Video Decoding │
    │  (DX11)     │  │ (FFmpeg D3D11VA)│
    └──────┬──────┘  └───┬────────────┘
           │              │
    ┌──────┴──────────────┴────────┐
    │   Desktop Integration        │
    │   (WorkerW Attachment)       │
    └──────────────────────────────┘
```

## Performance Targets

| Metric | Target |
|--------|--------|
| CPU usage (idle) | < 2% |
| GPU usage (1080p) | < 5% |
| Memory | < 150 MB |
| Game Mode latency | < 500 ms |

## License

MIT License - See [LICENSE](LICENSE) for details

## Contributing

Contributions welcome! Please open an issue first to discuss major changes.
