# Building Pixel Motion

## Prerequisites

Before building, ensure you have:

1. **Visual Studio 2022** with "Desktop development with C++" workload
   - Download from: https://visualstudio.microsoft.com/downloads/
   - During installation, select:
     - ✅ Desktop development with C++
     - ✅ Windows 11 SDK
     - ✅ C++ CMake tools for Windows

2. **vcpkg** (C++ package manager)
   ```powershell
   # Clone vcpkg
   git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
   cd C:\vcpkg
   
   # Bootstrap
   .\bootstrap-vcpkg.bat
   
   # Set environment variable
   [System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')
   ```

3. **Restart your terminal** after setting VCPKG_ROOT

---

## Build Steps

### Option 1: Visual Studio (Recommended)

1. **Open the project:**
   ```powershell
   cd d:\Github\Pixel-Motion
   ```

2. **Open in Visual Studio:**
   - File → Open → CMake...
   - Select `CMakeLists.txt`
   - Visual Studio will automatically configure with vcpkg

3. **Build:**
   - Build → Build All (Ctrl+Shift+B)
   - Or select build configuration (Debug/Release) and press F7

4. **Run:**
   - Debug → Start Without Debugging (Ctrl+F5)

### Option 2: Command Line

```powershell
cd d:\Github\Pixel-Motion

# Configure (first time only)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

# Build
cmake --build build --config Release

# Run
.\build\bin\Release\PixelMotion.exe
```

---

## Troubleshooting

### "vcpkg not found"

Ensure `VCPKG_ROOT` environment variable is set:
```powershell
$env:VCPKG_ROOT
# Should output: C:\vcpkg (or your installation path)
```

If not set:
```powershell
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')
# Restart terminal
```

### "FFmpeg not found"

vcpkg will automatically download and build FFmpeg on first configure. This may take 10-30 minutes.

To manually install:
```powershell
cd $env:VCPKG_ROOT
.\vcpkg install ffmpeg[avcodec,avformat,swscale]:x64-windows
```

### "fxc.exe not found"

The HLSL shader compiler (`fxc.exe`) is part of the Windows SDK. Ensure you have:
- Windows 11 SDK installed via Visual Studio Installer
- Path typically: `C:\Program Files (x86)\Windows Kits\10\bin\<version>\x64\fxc.exe`

### Build errors

1. **Clean and rebuild:**
   ```powershell
   Remove-Item -Recurse -Force build
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
   cmake --build build --config Release
   ```

2. **Check logs:**
   - Build output in Visual Studio Output window
   - CMake logs in `build/CMakeFiles/CMakeOutput.log`

---

## Current Limitations

> **⚠️ Important:** The video decoding system is currently a **stub**. The application will compile and run, but video playback is not yet functional.

**What works:**
- ✅ Application launches
- ✅ System tray icon appears
- ✅ Desktop integration (WorkerW attachment)
- ✅ Multi-monitor window creation
- ✅ DirectX 11 rendering (black screen)
- ✅ Game Mode detection
- ✅ Battery monitoring
- ✅ Logging to `%LOCALAPPDATA%\PixelMotion\logs`

**What's pending:**
- ⚠️ FFmpeg video decoding (stub)
- ⚠️ Audio playback (stub)
- ⚠️ Settings window UI (placeholder)
- ⚠️ Configuration file loading (basic)

---

## Testing the Current Build

1. **Launch the application:**
   ```powershell
   .\build\bin\Release\PixelMotion.exe
   ```

2. **Verify system tray:**
   - Look for "Pixel Motion" icon in system tray
   - Right-click → should show context menu

3. **Check logs:**
   ```powershell
   explorer "$env:LOCALAPPDATA\PixelMotion\logs"
   ```
   - Should see log file with initialization messages

4. **Test Game Mode:**
   - Launch a fullscreen game or press F11 in a browser
   - Check logs for "Fullscreen application detected"

5. **Test Battery Mode (if on laptop):**
   - Disconnect AC power
   - Check logs for "Switched to battery power"

---

## Next Development Steps

### 1. Implement FFmpeg Video Decoding

**File:** `src/video/VideoDecoder.cpp`

Key tasks:
- Uncomment FFmpeg includes
- Implement `Initialize()` with `avformat_open_input`
- Set up D3D11VA hardware context
- Implement `DecodeNextFrame()` with `av_read_frame`

**Resources:**
- [FFmpeg D3D11VA Example](https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/hw_decode.c)
- [Medium Article on D3D11VA](https://medium.com/@carmencincotti/integrating-ffmpeg-with-directx-11-a-step-by-step-guide-8b3e8e8e8e8e)

### 2. Connect Video to Renderer

**File:** `src/desktop/WallpaperWindow.cpp`

- Load video file in `Create()`
- Pass decoded frames to `m_renderer->SetVideoTexture()`
- Implement frame timing

### 3. Build Settings UI

**File:** `src/ui/SettingsWindow.cpp`

- Create dialog resource
- Implement monitor selection
- Add file browser for wallpaper selection

---

## Performance Monitoring

Once video playback is implemented, monitor:

```powershell
# CPU/GPU usage
Get-Process PixelMotion | Select-Object CPU, WorkingSet

# Or use Task Manager:
# - Performance tab
# - Look for "Pixel Motion" process
```

**Target metrics:**
- CPU: < 2% (idle desktop)
- GPU: < 5% (1080p video)
- Memory: < 150 MB

---

## Debugging

### Visual Studio Debugging

1. Set breakpoints in code
2. Debug → Start Debugging (F5)
3. Check Output window for logs

### Common breakpoint locations:

- `Application::Initialize()` - Startup
- `DesktopManager::FindWorkerW()` - Desktop integration
- `GameModeDetector::Update()` - Fullscreen detection
- `RendererContext::Render()` - Rendering loop

### Debug Output

All `Logger::Info/Warning/Error` calls output to:
- Visual Studio Output window (when debugging)
- `%LOCALAPPDATA%\PixelMotion\logs\*.log` files

---

## Distribution

Once complete, create a release build:

```powershell
cmake --build build --config Release
cmake --install build --prefix dist
```

Package contents:
```
dist/
├── bin/
│   ├── PixelMotion.exe
│   ├── shaders/
│   │   ├── FullscreenQuad.cso
│   │   └── NV12ToRGBA.cso
│   └── (FFmpeg DLLs if dynamic linking)
```

---

## Additional Resources

- **CMake Documentation:** https://cmake.org/documentation/
- **DirectX 11 Programming Guide:** https://docs.microsoft.com/en-us/windows/win32/direct3d11/
- **FFmpeg Documentation:** https://ffmpeg.org/documentation.html
- **vcpkg:** https://github.com/microsoft/vcpkg

---

## Support

For issues:
1. Check logs in `%LOCALAPPDATA%\PixelMotion\logs`
2. Review build output for errors
3. Verify all prerequisites are installed
4. Try clean rebuild
