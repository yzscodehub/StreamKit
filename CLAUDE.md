# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Quick Build
```batch
build.bat
```

### Manual Build
```batch
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="E:/WorkSpace/vcpkg/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Build Artifacts
- `build/Release/streamkit.exe` - Main performance testing tool
- `build/Release/seek_test.exe` - Performance summary analysis tool

## Architecture Overview

StreamKit is a C++ video streaming framework built on FFmpeg with NVIDIA GPU hardware acceleration. The architecture follows object-oriented design patterns with clear separation of concerns.

### Decoder Architecture (Factory Pattern)

The video decoding system uses a factory pattern with abstract base classes:

```
VideoDecoderBase (abstract)
├── SoftwareVideoDecoder (CPU-based)
└── HardwareVideoDecoder (GPU-based)
    ├── CUDA (NVIDIA NVDEC)
    ├── DXVA2/D3D11VA (Windows)
    ├── QSV (Intel)
    ├── VAAPI (Linux)
    └── VideoToolbox (macOS)
```

**Key Classes:**
- `VideoDecoderBase` (`include/video/decoder_base.hpp`) - Abstract interface defining common operations: `initialize()`, `decodeNextFrame()`, `seekToFrame()`, `close()`. Handles file I/O, stream management, frame indexing, statistics collection.
- `VideoDecoderFactory` (`include/video/decoder_factory.hpp`) - Creates decoders based on strategy: `PREFER_HARDWARE`, `PREFER_SOFTWARE`, `HARDWARE_ONLY`, `SOFTWARE_ONLY`, `AUTO_SELECT`
- `SoftwareVideoDecoder` (`include/video/software_decoder.hpp`) - CPU decoding with pixel format conversion (SwsContext)
- `HardwareVideoDecoder` (`include/video/hardware_decoder.hpp`) - GPU decoding with hardware frame transfer

### Core Processing Components

- **VideoEncoder** (`include/video/encoder.hpp`) - NVENC hardware encoding with configurable bitrate, FPS, GOP
- **TimestampManager** (`include/video/timestamp_manager.hpp`) - Global timebase management, multi-stream timestamp correction, A/V sync validation
- **CudaProcessor** (`include/cuda/cuda_processor.hpp`) - GPU memory management with memory pool, async CUDA processing, stream/event management, thread-safe design

### Singletons

- **Logger** (`include/common/logger.hpp`) - spdlog-based with console/file output (10MB rolling, 3 files). Macros: `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`
- **Config** (`include/common/config.hpp`) - JSON-based configuration with templated accessors

## Technical Details

### CUDA Configuration
- Target Architecture: Compute Capability 8.9 (RTX 4070 Ti / Ada Lovelace)
- C++ Standard: C++17 for both host and CUDA
- Required: CUDA Toolkit 11.0+

### Dependencies (vcpkg)
- FFmpeg 6.1 with nvcodec feature
- ffnvcodec (NVIDIA codec SDK)
- spdlog (logging)
- fmt (formatting)

### Hardware Requirements
- NVIDIA GPU with CUDA support
- Windows 10/11 primary platform (multi-platform hardware acceleration support)
- Visual Studio 2019/2022, CMake 3.20+

## Development Notes

### Adding New Decoders
1. Inherit from `HardwareVideoDecoder` or `SoftwareVideoDecoder`
2. Override `initialize()`, `decodeNextFrame()`, `seekToFrame()`, `close()`
3. Register in `VideoDecoderFactory` by adding to `DecoderType` enum and factory methods

### Performance Characteristics
- Hardware decoding on RTX 4070 Ti: 3-5x faster than software for H.264/HEVC
- Key design goals: Low latency, efficient frame indexing for random access, smart buffering
- Memory management: GPU memory pools, async GPU/CPU parallel processing

### Main Executable Purpose
`src/main.cpp` is a **performance testing tool** that:
- Detects hardware decoder support
- Runs comprehensive seek performance tests (random, sequential, forward/backward, short/long distance, keyframe/non-keyframe)
- Generates decoding statistics and comparisons
- Outputs performance metrics to console

### Namespaces
All code lives under `StreamKit` namespace.

### Logging
Always use the Logger macros instead of raw spdlog:
```cpp
LOG_INFO("Message: {}", variable);
LOG_ERROR("Error occurred: {}", error_code);
```
