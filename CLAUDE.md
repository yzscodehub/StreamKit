# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**PhoenixEditor** is a modern C++20 **non-linear video editor (NLE)** featuring:
- Qt6/QML-based timeline UI with drag-and-drop editing
- FFmpeg-powered media decoding/encoding
- Compositor-based frame rendering (on-demand frame requests)
- Undo/Redo with Command pattern
- Plugin-ready architecture for effects and codecs

**Current Status**: 
- ✅ Phase 1-3: MVP player with audio/video, user controls, pre-roll state machine
- 🚧 Phase 4: Editor core (data model, compositor, timeline)
- ⏳ Phase 5: Effects, transitions, export

## 1. Tech Stack

### Language & Build
- **Language**: C++20 (concepts, variant, format, ranges)
- **Build System**: CMake 3.21+ with presets
- **Package Manager**: vcpkg
- **Build Command**: `cmake --preset debug && cmake --build --preset debug`
- **Editor Build**: `cmake --preset editor-debug && cmake --build --preset editor-debug`
- **Test Framework**: GTest (`-DPHOENIX_BUILD_TESTS=ON`)

### UI Layer
- **Framework**: Qt 6.6+ / QML
- **Theme**: Material Design (Dark)
- **Components**: Timeline, VideoPreview, MediaBin, Sidebar, TransportControls

### Media Layer
- **Decoding**: FFmpeg 5.0+ (libavcodec 59+, send/receive API)
- **Encoding**: FFmpeg (export pipeline)
- **Audio**: SDL2 (callback-driven playback)
- **Thumbnails**: FFmpeg frame extraction

### Data & Persistence
- **Project Format**: JSON (nlohmann/json)
- **Undo/Redo**: Command pattern with UndoStack

### Logging
- **Logging**: spdlog + fmt

### Platform Support
- **Primary**: Windows 10/11 (x64)
- **Future**: Linux, macOS

## 2. Architecture Overview

### Module Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                       应用层 (apps/)                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐              │
│  │   editor    │  │   player    │  │   render    │              │
│  │  (Qt GUI)   │  │  (SDL CLI)  │  │ (Headless)  │              │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘              │
└─────────┼────────────────┼────────────────┼─────────────────────┘
          │                │                │
          ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      核心库 (phoenix/)                           │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                     engine                               │    │
│  │   Compositor | PlaybackEngine | ExportEngine | Cache     │    │
│  └─────────────────────────┬───────────────────────────────┘    │
│                            │                                     │
│        ┌───────────────────┼───────────────────┐                │
│        ▼                   ▼                   ▼                │
│  ┌──────────┐       ┌──────────┐       ┌──────────┐             │
│  │  model   │       │  media   │       │ effects  │             │
│  │ Project  │       │ Decoder  │       │ Effect   │             │
│  │ Sequence │       │ Encoder  │       │ Registry │             │
│  │ Clip     │       │ Pool     │       │ Plugins  │             │
│  └────┬─────┘       └────┬─────┘       └────┬─────┘             │
│       │                  │                  │                    │
│       └──────────────────┼──────────────────┘                    │
│                          ▼                                       │
│                   ┌──────────┐                                   │
│                   │   core   │                                   │
│                   │  Types   │                                   │
│                   │  Result  │                                   │
│                   │  Clock   │                                   │
│                   └──────────┘                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Directory Structure

```
StreamKit/
├── CMakeLists.txt              # 顶层 CMake
├── CMakePresets.json
├── vcpkg.json
│
├── phoenix/                    # ══════ 核心库 ══════
│   │
│   ├── core/                   # 基础设施层 (仅依赖 spdlog/fmt)
│   │   ├── CMakeLists.txt
│   │   ├── include/phoenix/core/
│   │   │   ├── types.hpp           # Timestamp, Duration, Rational
│   │   │   ├── result.hpp          # Result<T, E>
│   │   │   ├── uuid.hpp            # UUID 生成
│   │   │   ├── clock.hpp           # SeqLock MasterClock
│   │   │   ├── logger.hpp          # 日志封装 (spdlog 薄封装)
│   │   │   ├── ring_buffer.hpp     # 无锁环形缓冲
│   │   │   ├── lru_cache.hpp       # LRU 缓存模板
│   │   │   └── signals.hpp         # 轻量信号槽 (非 Qt)
│   │   └── src/
│   │       ├── uuid.cpp
│   │       ├── clock.cpp
│   │       └── logger.cpp
│   │
│   ├── model/                  # 数据模型层 (无 FFmpeg 依赖)
│   │   ├── CMakeLists.txt
│   │   ├── include/phoenix/model/
│   │   │   ├── project.hpp         # 项目容器
│   │   │   ├── media_item.hpp      # 素材元数据
│   │   │   ├── media_bin.hpp       # 素材库
│   │   │   ├── sequence.hpp        # 时间线
│   │   │   ├── track.hpp           # 轨道
│   │   │   ├── clip.hpp            # 片段
│   │   │   ├── commands/           # Undo/Redo
│   │   │   │   ├── command.hpp
│   │   │   │   ├── undo_stack.hpp
│   │   │   │   └── clip_commands.hpp
│   │   │   └── io/
│   │   │       └── project_io.hpp
│   │   └── src/
│   │       ├── project.cpp
│   │       ├── sequence.cpp
│   │       └── clip.cpp
│   │
│   ├── media/                  # 媒体处理层 (FFmpeg PIMPL + 软硬件编解码)
│   │   ├── CMakeLists.txt
│   │   ├── include/phoenix/media/
│   │   │   ├── codec_types.hpp     # HWAccelType, CodecPreference 枚举
│   │   │   ├── codec_capability.hpp # 硬件能力检测 (单例)
│   │   │   ├── frame.hpp           # VideoFrame, AudioFrame (支持 GPU 帧)
│   │   │   ├── decoder.hpp         # 统一解码器 (PIMPL, 软硬件透明)
│   │   │   ├── encoder.hpp         # 统一编码器 (PIMPL, 软硬件透明)
│   │   │   ├── decoder_pool.hpp    # 解码器池
│   │   │   ├── media_info.hpp      # 文件信息探测
│   │   │   └── thumbnail.hpp       # 缩略图生成
│   │   └── src/
│   │       ├── codec_capability.cpp
│   │       ├── decoder.cpp
│   │       ├── encoder.cpp
│   │       └── ffmpeg/             # FFmpeg 内部封装 (不暴露)
│   │           ├── ff_utils.hpp/.cpp
│   │           ├── shared_avframe.hpp
│   │           ├── ff_hw_device.hpp/.cpp    # 硬件设备管理
│   │           ├── ff_decoder_impl.hpp/.cpp # 解码器实现
│   │           └── ff_encoder_impl.hpp/.cpp # 编码器实现
│   │
│   ├── effects/                # 特效系统层 (Phase 5)
│   │   └── ...
│   │
│   └── engine/                 # 渲染引擎层
│       ├── CMakeLists.txt
│       ├── include/phoenix/engine/
│       │   ├── compositor.hpp      # 多层合成器
│       │   ├── frame_cache.hpp     # 帧缓存 (LRU)
│       │   └── playback/
│       │       └── playback_engine.hpp
│       └── src/
│           ├── compositor.cpp
│           └── playback/
│               └── playback_engine.cpp
│
├── apps/                       # ══════ 应用程序 ══════
│   │
│   ├── editor/                 # Qt/QML 编辑器
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.cpp
│   │   │   └── controllers/
│   │   │       ├── project_controller.hpp/.cpp
│   │   │       ├── timeline_controller.hpp/.cpp
│   │   │       └── preview_controller.hpp/.cpp
│   │   ├── qml/
│   │   │   ├── Main.qml
│   │   │   ├── Timeline.qml
│   │   │   └── ...
│   │   └── resources/
│   │
│   ├── player/                 # SDL 独立播放器 (Legacy)
│   │   └── src/
│   │       └── main.cpp
│   │
│   └── render/                 # 无头渲染器 (CLI)
│       └── src/
│           └── main.cpp
│
├── src/                        # [LEGACY] 旧代码，待迁移后删除
│   ├── graph/                  # Legacy Flow Graph
│   ├── nodes/                  # Legacy Player Nodes
│   └── render/                 # Legacy SDL Renderer
│
└── .claude/
    └── current_plan.md         # 当前执行计划
```

### Module Dependencies

| 模块 | 职责 | 依赖 | 外部依赖 |
|------|------|------|----------|
| `phoenix/core` | 基础类型、Result、Clock、UUID、Logger | **无** | spdlog, fmt |
| `phoenix/model` | Project/Sequence/Clip、Undo/Redo、JSON | core | nlohmann/json |
| `phoenix/media` | 软硬件编解码、DecoderPool、Encoder | core | FFmpeg (+nvcodec, qsv) |
| `phoenix/effects` | 特效接口、内置特效 | core, media | - |
| `phoenix/engine` | Compositor、FrameCache、PlaybackEngine | core, model, media | - |
| `apps/editor` | Qt Controllers、QML UI | 所有 phoenix/* | Qt6 |
| `apps/player` | 独立播放器 (Legacy) | phoenix/core, media | SDL2 |

### Hardware Acceleration Support

| 平台 | 解码 (Decode) | 编码 (Encode) |
|------|---------------|---------------|
| Windows NVIDIA | NVDEC (CUDA, D3D11VA) | NVENC |
| Windows Intel | QSV, D3D11VA | QSV |
| Windows AMD | D3D11VA | AMF |
| Linux | VAAPI, NVDEC | VAAPI, NVENC |
| macOS | VideoToolbox | VideoToolbox |
| `apps/render` | CLI 导出工具 | phoenix/engine | - |

### Key Data Models

```cpp
// Project structure
Project
├── MediaBin (imported media items)
├── Sequences[] (timelines)
│   ├── VideoTracks[]
│   │   └── Clips[]
│   │       ├── MediaItemRef
│   │       ├── timelineIn/Out
│   │       ├── sourceIn/Out
│   │       ├── speed, reversed
│   │       └── Effects[]
│   └── AudioTracks[]
└── ProjectSettings (resolution, frameRate)
```

### Frame Request Model (Editor vs Player)

**Player (Push Model)**: `Source → Queue → Decoder → Queue → Sink`

**Editor (Pull Model)**: 
```
Timeline needs frame at T
    → Compositor finds active Clips at T
    → DecoderPool seeks/decodes each source
    → Effects applied
    → Composited frame returned
```

## 3. Coding Standards

### Naming Conventions
- **Files**: `snake_case` (e.g., `decoder_pool.hpp`)
- **Classes**: `PascalCase` (e.g., `DecoderPool`)
- **Functions/Methods**: `camelCase` (e.g., `getFrame()`)
- **Variables**: `camelCase` (e.g., `frameCount`)
- **Private Members**: `m_` prefix (e.g., `m_frameCache`, `m_decoder`)
- **Constants**: `k` prefix + `PascalCase` (e.g., `kMaxTracks`)
- **Namespaces**: `snake_case` (e.g., `phoenix::model`)

### File Organization
| 内容类型 | 放置位置 | 示例 |
|---------|---------|------|
| 类声明 | `.hpp` | `class Clip { ... };` |
| 简单 getter/setter | `.hpp` (inline) | `UUID id() const { return m_id; }` |
| 模板代码 | `.hpp` | `template<T> class Result` |
| constexpr 函数 | `.hpp` | `constexpr int kMaxTracks = 32;` |
| 复杂成员函数 | `.cpp` | `void Clip::applySpeed(float)` |
| 构造/析构函数 | `.cpp` | `Clip::Clip()` |
| 第三方库代码 | `.cpp` | FFmpeg 相关 (PIMPL) |

### Error Handling
- Use `Result<T, E>` variant type for error propagation (no exceptions for regular errors)
- Only fatal errors throw exceptions

### Other Rules
- **Comments**: Explain "Why", not "What"
- **RAII**: Mandatory - no manual resource management
- **Undo/Redo**: All editing operations must go through Command pattern

## 4. Workflow Rules

- **Planning First**: Don't code without a plan
- **Verification**: Always run the **Test Command** before submitting changes
- **No Hallucinations**: Do not use APIs or libraries not listed in `vcpkg.json`

## 5. Communication Rules

- **Input Language**: The user may speak Chinese or English
- **Output Language**: **ALWAYS** reply in the **Same Language** used by the user
  - If user asks in Chinese → Reply in Chinese
  - If user asks in English → Reply in English
- **Code Language**: Code, variable names, and comments inside code must remain in **English**

## Build Commands

### Using CMake Presets (Recommended)
```bash
# Debug build (player only)
cmake --preset debug
cmake --build --preset debug

# Release build
cmake --preset release
cmake --build --preset release

# With Qt/QML editor
cmake --preset editor-debug
cmake --build --preset editor-debug
```

### Using build.bat (Windows)
```bash
.\build.bat debug
.\build.bat release
.\build.bat clean
```

### Running
```bash
# Player mode
cd build\Debug
.\PhoenixPlayer.exe <video_file.mp4>

# Editor mode
.\PhoenixEditor.exe
```

### Player Controls
- `Space`: Pause/Resume
- `←/→`: Seek ±5 seconds
- `Q` or `ESC`: Quit

## Testing

Tests are configured but not yet implemented. To enable:
```bash
cmake -DPHOENIX_BUILD_TESTS=ON ..
```

Test framework: GTest (via `PHOENIX_BUILD_TESTS` option)

## Critical Design Patterns

### 1. Command Pattern for Undo/Redo
```cpp
class Command {
public:
    virtual void execute() = 0;
    virtual void undo() = 0;
    virtual std::string description() const = 0;
};

class UndoStack {
public:
    void execute(std::unique_ptr<Command> cmd);
    void undo();
    void redo();
};
```

### 2. Result<T,E> Error Handling (`phoenix/core/include/phoenix/core/result.hpp`)
- No exceptions for regular errors
- `Result<T, E>` variant type for error propagation
- Only fatal errors throw exceptions

### 3. SeqLock MasterClock (`phoenix/core/include/phoenix/core/clock.hpp`)
- Lock-free reads in hot paths
- Single writer, multiple readers
- `update()` for writer, `now()` for readers

### 4. Compositor Frame Request
```cpp
class Compositor {
public:
    std::future<VideoFrame> requestFrame(
        const Sequence* sequence,
        Timestamp time,
        RenderQuality quality = RenderQuality::Preview
    );
};
```

### 5. DecoderPool (Multi-source management)
- Reuses decoders for same media source
- Seek optimization with position caching
- Thread-safe acquire/release

### 6. PIMPL Pattern (Hide FFmpeg)
```cpp
// decoder.hpp - public header, no FFmpeg includes
class Decoder {
public:
    Decoder();
    ~Decoder();
    Result<VideoFrame, Error> decodeFrame(Timestamp time);
private:
    struct Impl;  // FFmpeg details hidden
    std::unique_ptr<Impl> m_impl;
};
```

## Important Constraints

### FFmpeg Integration
- **Use FFmpeg 5.0+ API only**: `avcodec_send_packet()` + `avcodec_receive_frame()`
- **SharedAVFrame**: Use `av_frame_ref` for copying, NOT `std::shared_ptr`
- **TimeBase**: Convert to int64_t microseconds at source boundary using `av_rescale_q`
- **Channel Layout**: Use `AVChannelLayout` struct + `swr_alloc_set_opts2()` (not deprecated uint64_t)
- **PIMPL**: All FFmpeg headers must be in `.cpp` files only

### Thread Safety
- SDL audio callback runs on separate thread - use lock-free ring buffer
- MasterClock uses SeqLock for lock-free reads
- All state checks should use atomics
- Never hold mutex in audio callback

### Legacy Player Constraints (apps/player/)
- Video queue: 50 packets
- Audio queue: 1000 packets
- Ring buffer: 128KB (~680ms at 48kHz stereo S16)
- Pre-roll timeout: 500ms

## Build Configuration

**Dependencies** (managed via vcpkg):
- FFmpeg 5.0+ with features: all, nvcodec, qsv, x264, x265
- SDL2 for audio output
- Qt6 for editor UI
- spdlog for logging
- fmt for formatting
- nlohmann-json for project files

**Platform specifics**:
- Windows: Links d3d11, dxgi for future hardware acceleration
- C++20 required (concepts, ranges)

**vcpkg path in CMakePresets.json**: Update `VCPKG_ROOT` if different from default

## Known Issues / Gotchas

1. **Hardware decode context lifetime**: Hardware frames tied to device context. If decoder destroyed while sink holds frames → crash.

2. **1-to-N decoding**: Single packet can produce 0, 1, or N frames. Must loop `avcodec_receive_frame()` until `EAGAIN`.

3. **EOF draining**: On EOF packet, send NULL to codec to enter drain mode, loop until `AVERROR_EOF`.

4. **Frame drop location**: Drop at Sink (post-decode) is SAFE. Drop at Decoder (pre-decode) is DANGEROUS.

5. **EAGAIN is not an error**: After `avcodec_send_packet()`, `EAGAIN` means "need more input" - normal state.

## Documentation Files

- `.claude/current_plan.md`: Current execution plan with detailed tasks
- `EXIT_MECHANISMS.md`: Exit mechanism documentation
- `Plant.md`: Legacy architecture design with flow diagrams

## Development Tips

- All timestamps use int64_t microseconds for precision
- Use `Result<T, E>` instead of exceptions for expected errors
- RAII is mandatory - no manual resource management
- All editing operations should create Commands for undo support
- Compositor uses pull model (request frames) vs player's push model (stream frames)
- Editor and Player share media layer but have different render engines
- Private members use `m_` prefix: `m_decoder`, `m_cache`, `m_position`