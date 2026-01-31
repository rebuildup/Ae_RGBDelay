# RGBDelay

An Adobe After Effects plugin that applies temporal delay to individual RGB color channels, creating chromatic aberration and glitch effects.

## Features

- **Per-Channel Delay**: Apply independent delay values to Red, Green, and Blue channels
- **8-bit & 16-bit Support**: Works with both 8-bit and 16-bit per channel color depth
- **Wide Time Input**: Samples frames across the entire composition timeline
- **Threaded Rendering**: Optimized for multi-core CPUs via After Effects' threading engine
- **Fast Path Optimization**: Direct copy when all channels sample the same frame

## Installation

### Windows

1. Build the project using Visual Studio (see Build Instructions below)
2. Copy the resulting `.aex` file to:
   ```
   C:\Program Files\Adobe\Adobe After Effects [version]\Support Files\Plug-ins\Effects\
   ```
3. Restart After Effects

### macOS

1. Build the project using Xcode (see Build Instructions below)
2. Copy the resulting `.plugin` bundle to:
   ```
   /Applications/Adobe After Effects [version]/Plug-ins/Effects/
   ```
3. Restart After Effects

## Usage

1. Apply the **RGBDelay** effect to any layer in After Effects
2. Adjust the delay parameters:
   - **Red Delay**: Frames to offset the red channel (-100 to +100)
   - **Green Delay**: Frames to offset the green channel (-100 to +100)
   - **Blue Delay**: Frames to offset the blue channel (-100 to +100)
3. Positive values sample earlier frames, negative values sample later frames

### Example Use Cases

- **Chromatic Aberration**: Set different small delays (1-3 frames) for each channel
- **Glitch Effects**: Use large, varying delays to create digital distortion
- **Temporal Color Separation**: Create rainbow trails by animating delay values

## Build Instructions

### Prerequisites

- **Windows**: Visual Studio 2019 or later with C++ support
- **macOS**: Xcode 12 or later
- **After Effects SDK**: Download from [Adobe's GitHub](https://github.com/adobe/after-effects-sdk)

### Windows (Visual Studio)

1. Open `Win/RGBDelay.sln`
2. Select Release or Debug configuration
3. Build solution (F7)
4. Output: `Win/Release/RGBDelay.aex` or `Win/Debug/RGBDelay.aex`

### macOS (Xcode)

1. Open `Mac/RGBDelay.xcodeproj`
2. Select RGBDelay scheme
3. Build product (Cmd+B)
4. Output: `Mac/Build/Release/RGBDelay.plugin`

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Red Delay | -100 to 100 | 0 | Number of frames to offset the red channel |
| Green Delay | -100 to 100 | 0 | Number of frames to offset the green channel |
| Blue Delay | -100 to 100 | 0 | Number of frames to offset the blue channel |

## Technical Details

- **Version**: 1.0.0
- **Match Name**: `361do_RGBDelay`
- **Category**: `361do`
- **Thread Safety**: Uses After Effects' iterate suites for thread-safe rendering
- **Memory**: Caches up to 3 unique time samples per render

## License

Copyright (C) 2024 Tsuyoshi Okumura/Hotkey ltd.

MIT License - See [LICENSE.txt](LICENSE.txt) for details

## Support

- **GitHub**: https://github.com/rebuildup/Ae_RGBDelay
- **Issues**: Report bugs via GitHub Issues

## Credits

Developed by Tsuyoshi Okumura / Hotkey ltd.

Built with Adobe After Effects SDK
