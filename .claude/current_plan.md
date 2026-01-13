# PhoenixEditor Phase 4 执行计划

> **Planner**: Product & Task Planner  
> **Architect**: System Architect  
> **Last Updated**: 2026-01-13  
> **Status**: Phase 4 - 编辑器核心开发

---

## 0. Goal Summary (目标摘要)

### 项目背景
PhoenixEditor 是一个 C++20 非线性视频编辑器。Phase 1-3 已完成 MVP 播放器。
**Phase 4 的目标是实现编辑器核心功能**。

### Phase 4 目标
> **让用户能够导入视频、在时间线上编辑、预览播放、保存/加载项目。**

### 具体交付物
1. ✅ 模块化代码结构 (`phoenix/` + `apps/`)
2. ✅ 数据模型 (Project/Sequence/Track/Clip)
3. ✅ 合成器引擎 (Compositor)
4. ✅ Undo/Redo 系统
5. ✅ QML 集成 (Controllers)
6. ✅ 项目文件保存/加载 (JSON)

### 不在 Phase 4 范围
- ❌ 特效系统 (Phase 5)
- ❌ 转场动画 (Phase 5)
- ❌ 视频导出 (Phase 5)
- ❌ 关键帧动画 (Phase 5)
- ❌ 硬件加速 (Phase 5+)

---

## 1. User Acceptance Criteria (用户验收标准)

### UAC-1: 媒体导入
- [ ] **用户可以**通过菜单 File → Open 选择视频文件
- [ ] **用户可以**在 Sidebar 的 MediaBin 中看到导入的素材
- [ ] **用户可以**看到素材的缩略图和时长信息

### UAC-2: 时间线编辑
- [ ] **用户可以**将素材从 MediaBin 拖拽到 Timeline
- [ ] **用户可以**在 Timeline 上看到 Clip 显示
- [ ] **用户可以**拖拽移动 Clip 的位置
- [ ] **用户可以**拖拽 Clip 边缘进行裁剪 (Trim)
- [ ] **用户可以**选中并删除 Clip

### UAC-3: 预览播放
- [ ] **用户可以**点击播放按钮预览时间线内容
- [ ] **用户可以**暂停/继续播放
- [ ] **用户可以**拖拽 Playhead 跳转到任意位置
- [ ] **用户可以**看到当前帧画面在 VideoPreview 中显示

### UAC-4: Undo/Redo
- [ ] **用户可以**按 Ctrl+Z 撤销上一步操作
- [ ] **用户可以**按 Ctrl+Y 重做已撤销的操作
- [ ] **用户可以**多次连续撤销

### UAC-5: 项目保存/加载
- [ ] **用户可以**通过 File → Save 保存项目为 .phoenix 文件
- [ ] **用户可以**通过 File → Open 加载已保存的项目
- [ ] **用户可以**关闭时看到"未保存"提示

### UAC-6: 稳定性
- [ ] **程序不会**因为快速操作而崩溃
- [ ] **程序不会**出现内存泄漏 (连续使用 30 分钟)
- [ ] **程序保持**响应 (UI 不卡顿超过 500ms)

---

## 2. Step-by-Step Implementation Plan (实施计划)

### Milestone Overview

```
Week 1         Week 2         Week 3         Week 4
  │              │              │              │
  ▼              ▼              ▼              ▼
┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐
│M1: 基础│ → │M2: 模型│ → │M3: 引擎│ → │M4: 集成│
│目录+Core│   │数据结构│   │合成器  │   │UI+测试 │
└────────┘   └────────┘   └────────┘   └────────┘
```

---

### M1: 基础设施 (Day 1-3)

**目标**: 建立模块化目录结构，迁移 Core 模块

| 任务 | 验收标准 |
|------|----------|
| 创建 `phoenix/` + `apps/` 目录 | 目录存在 |
| 迁移 Core 模块 (types, result, clock) | 编译通过 |
| 迁移 Player 到 `apps/player/` | `PhoenixPlayer.exe` 正常播放 |
| 新增 UUID 生成模块 | 可生成唯一 ID |

**检查点**: 
- [ ] `cmake --preset debug` 成功
- [ ] Player 播放视频正常

---

### M2: 数据模型 (Day 4-8)

**目标**: 实现 Project/Sequence/Track/Clip 数据结构

| 任务 | 验收标准 |
|------|----------|
| 实现 MediaItem 结构 | 可存储素材路径、时长 |
| 实现 Clip 结构 | 可设置 timelineIn/Out |
| 实现 Track 结构 | 可添加/删除 Clip |
| 实现 Sequence 结构 | 可管理多个 Track |
| 实现 Project 结构 | 包含 MediaBin + Sequences |
| 实现 JSON 序列化 | Project 可保存/加载 |

**检查点**:
- [ ] 单元测试: Clip 添加到 Track
- [ ] 单元测试: Project 序列化往返

---

### M3: 渲染引擎 (Day 9-15)

**目标**: 实现 Compositor 和 PlaybackEngine

| 任务 | 验收标准 |
|------|----------|
| 迁移 FFmpeg 封装到 `phoenix/media/` | 编译通过 |
| 实现 Decoder (PIMPL) | 可解码视频帧 |
| 实现 DecoderPool | 复用解码器实例 |
| 实现 FrameCache (LRU) | 缓存命中正常 |
| 实现 Compositor | `renderFrame(seq, time)` 返回帧 |
| 实现 PlaybackEngine | 可播放/暂停 |

**检查点**:
- [ ] 集成测试: Compositor 渲染单帧
- [ ] 集成测试: PlaybackEngine 连续播放

---

### M4: 命令系统 + UI 集成 (Day 16-24)

**目标**: 实现 Undo/Redo，集成 QML 控制器

| 任务 | 验收标准 |
|------|----------|
| 实现 Command 基类 | 支持 execute/undo |
| 实现 UndoStack | 可 undo/redo |
| 实现 AddClipCommand | 添加 Clip 可撤销 |
| 实现 MoveClipCommand | 移动 Clip 可撤销 |
| 实现 TrimClipCommand | 裁剪 Clip 可撤销 |
| 实现 DeleteClipCommand | 删除 Clip 可撤销 |
| 实现 ProjectController | 媒体导入、保存加载 |
| 实现 TimelineController | Clip 操作 |
| 实现 PreviewController | 播放控制 |
| 更新 QML 绑定 | UI 可交互 |
| 端到端测试 | 完整工作流验证 |

**检查点**:
- [ ] UAC-1 ~ UAC-6 全部通过

---

## 3. Risks & Dependencies (风险与依赖)

### 依赖项

| 依赖 | 状态 | 说明 |
|------|------|------|
| Qt 6.6+ | ✅ 已安装 | QML UI |
| FFmpeg 5.0+ | ✅ 已安装 | 解码编码 |
| nlohmann/json | ✅ 在 vcpkg.json | JSON 序列化 |
| spdlog | ✅ 已安装 | 日志 |

### 风险评估

| 风险 | 可能性 | 影响 | 缓解措施 |
|------|--------|------|----------|
| FFmpeg PIMPL 迁移复杂 | 中 | +2 天 | 保留现有接口 |
| Compositor 性能不足 | 中 | 影响体验 | 先功能后优化 |
| QML 绑定调试困难 | 低 | +1 天 | 参考现有 VideoController |
| 跨模块依赖错误 | 中 | 编译失败 | 按顺序开发 |

### 关键假设
1. 现有 Player 代码的 FFmpeg 封装可复用
2. Qt/QML 已有的 UI 骨架可直接使用
3. 不需要支持多 Sequence 编辑 (Phase 4 只支持单个)

---

## 4. Success Metrics (成功指标)

| 指标 | 目标值 |
|------|--------|
| UAC 通过率 | 100% (6/6) |
| 单元测试覆盖率 | > 60% (Model 层) |
| 启动时间 | < 3 秒 |
| 预览帧率 | > 24 fps |
| 内存占用 | < 500 MB (空项目) |
| 崩溃次数 | 0 (正常使用) |

---

## 5. Timeline Summary (时间线)

| Week | Focus | Deliverable |
|------|-------|-------------|
| Week 1 | 基础 + Model | 目录结构, 数据模型 |
| Week 2 | Engine | Compositor, PlaybackEngine |
| Week 3 | Commands + Controllers | Undo/Redo, QML 集成 |
| Week 4 | Integration + Test | UAC 验证, Bug 修复 |

**总预计时间**: 4 周 (20 工作日)

---

---

# Appendix: Technical Design (技术设计附录)

> 以下为 Architect 的技术设计细节，供实施参考。

---

## A1. 整体架构

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

---

## 2. 目录结构

```
StreamKit/
├── CMakeLists.txt              # 顶层 CMake
├── CMakePresets.json
├── vcpkg.json
├── build.bat
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
│   │   │   ├── thread_pool.hpp     # 任务调度
│   │   │   ├── ring_buffer.hpp     # 无锁环形缓冲
│   │   │   ├── lru_cache.hpp       # LRU 缓存模板
│   │   │   └── signals.hpp         # 轻量信号槽 (非 Qt)
│   │   ├── src/
│   │   │   ├── uuid.cpp
│   │   │   ├── clock.cpp
│   │   │   └── logger.cpp          # 日志初始化
│   │   └── tests/
│   │       └── test_result.cpp
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
│   │   │   ├── effect_instance.hpp # 特效实例
│   │   │   ├── transition.hpp      # 转场
│   │   │   │
│   │   │   ├── commands/           # Undo/Redo
│   │   │   │   ├── command.hpp
│   │   │   │   ├── undo_stack.hpp
│   │   │   │   └── clip_commands.hpp
│   │   │   │
│   │   │   └── io/                 # 序列化
│   │   │       └── project_io.hpp
│   │   ├── src/
│   │   │   ├── project.cpp
│   │   │   ├── sequence.cpp
│   │   │   ├── clip.cpp
│   │   │   └── commands/
│   │   │       └── clip_commands.cpp
│   │   └── tests/
│   │       └── test_clip.cpp
│   │
│   ├── media/                  # 媒体处理层 (FFmpeg 封装)
│   │   ├── CMakeLists.txt
│   │   ├── include/phoenix/media/
│   │   │   ├── media_info.hpp      # 文件信息探测
│   │   │   ├── frame.hpp           # VideoFrame, AudioFrame
│   │   │   ├── decoder.hpp         # 单解码器
│   │   │   ├── decoder_pool.hpp    # 解码器池
│   │   │   ├── encoder.hpp         # 编码器
│   │   │   ├── thumbnail.hpp       # 缩略图生成
│   │   │   └── waveform.hpp        # 波形生成
│   │   ├── src/
│   │   │   ├── decoder.cpp
│   │   │   ├── decoder_pool.cpp
│   │   │   ├── encoder.cpp
│   │   │   └── ffmpeg/             # FFmpeg 内部封装
│   │   │       ├── ff_utils.hpp
│   │   │       ├── ff_utils.cpp
│   │   │       └── shared_avframe.hpp
│   │   └── tests/
│   │       └── test_decoder.cpp
│   │
│   ├── effects/                # 特效系统层
│   │   ├── CMakeLists.txt
│   │   ├── include/phoenix/effects/
│   │   │   ├── effect.hpp          # 特效基类接口
│   │   │   ├── effect_registry.hpp # 特效注册表
│   │   │   ├── parameter.hpp       # 参数定义
│   │   │   │
│   │   │   └── builtin/            # 内置特效
│   │   │       ├── transform.hpp
│   │   │       ├── opacity.hpp
│   │   │       └── blur.hpp
│   │   ├── src/
│   │   │   ├── effect_registry.cpp
│   │   │   └── builtin/
│   │   │       └── transform.cpp
│   │   └── tests/
│   │       └── test_effects.cpp
│   │
│   └── engine/                 # 渲染引擎层
│       ├── CMakeLists.txt
│       ├── include/phoenix/engine/
│       │   ├── compositor.hpp      # 多层合成器
│       │   ├── frame_cache.hpp     # 帧缓存 (LRU)
│       │   ├── render_context.hpp  # 渲染上下文
│       │   │
│       │   ├── playback/
│       │   │   ├── playback_engine.hpp
│       │   │   └── audio_mixer.hpp
│       │   │
│       │   └── export/
│       │       ├── export_engine.hpp
│       │       └── export_settings.hpp
│       ├── src/
│       │   ├── compositor.cpp
│       │   ├── frame_cache.cpp
│       │   └── playback/
│       │       └── playback_engine.cpp
│       └── tests/
│           └── test_compositor.cpp
│
├── apps/                       # ══════ 应用程序 ══════
│   │
│   ├── editor/                 # Qt/QML 编辑器
│   │   ├── CMakeLists.txt
│   │   ├── src/
│   │   │   ├── main.cpp
│   │   │   └── controllers/
│   │   │       ├── project_controller.hpp
│   │   │       ├── project_controller.cpp
│   │   │       ├── timeline_controller.hpp
│   │   │       ├── timeline_controller.cpp
│   │   │       ├── preview_controller.hpp
│   │   │       └── preview_controller.cpp
│   │   ├── qml/
│   │   │   ├── Main.qml
│   │   │   ├── Timeline.qml
│   │   │   ├── VideoPreview.qml
│   │   │   ├── Sidebar.qml
│   │   │   └── TransportControls.qml
│   │   └── resources/
│   │       └── icons/
│   │
│   ├── player/                 # SDL 独立播放器 (Legacy)
│   │   ├── CMakeLists.txt
│   │   └── src/
│   │       ├── main.cpp
│   │       └── player.hpp      # 原有 Pipeline 播放器
│   │
│   └── render/                 # 无头渲染器 (CLI)
│       ├── CMakeLists.txt
│       └── src/
│           └── main.cpp        # phoenix-render project.json -o out.mp4
│
├── docs/                       # ══════ 文档 ══════
│   └── ARCHITECTURE.md
│
└── .claude/                    # ══════ AI 辅助 ══════
    ├── CLAUDE.md
    └── current_plan.md
```

---

## 3. 模块职责定义

| 模块 | 职责 | 依赖 | 外部依赖 |
|------|------|------|----------|
| `phoenix/core` | 基础类型、Result、Clock、UUID、Logger | **无** | spdlog, fmt |
| `phoenix/model` | Project/Sequence/Clip 数据模型、Undo/Redo、JSON 序列化 | core | nlohmann/json |
| `phoenix/media` | FFmpeg 封装、DecoderPool、Encoder、Thumbnails | core | FFmpeg |
| `phoenix/effects` | 特效接口、内置特效、特效注册表 | core, media | - |
| `phoenix/engine` | Compositor、FrameCache、PlaybackEngine、ExportEngine | core, model, media, effects | - |
| `apps/editor` | Qt Controllers、QML UI | 所有 phoenix/* | Qt6 |
| `apps/player` | 独立播放器 (Legacy Pipeline) | phoenix/core, phoenix/media | SDL2 |
| `apps/render` | CLI 导出工具 | phoenix/core, model, engine | - |

---

## 4. 模块依赖图

```
                    ┌─────────────────┐
                    │  phoenix/core   │  无外部依赖
                    │  (header-only?) │
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
     ┌────────────┐  ┌────────────┐  ┌────────────┐
     │phoenix/model│  │phoenix/media│  │            │
     │ (JSON)      │  │ (FFmpeg)    │  │            │
     └──────┬─────┘  └──────┬─────┘  │            │
            │               │         │            │
            │               ▼         │            │
            │        ┌────────────┐   │            │
            │        │phoenix/    │   │            │
            │        │effects     │◄──┘            │
            │        └──────┬─────┘                │
            │               │                      │
            └───────────────┼──────────────────────┘
                            │
                            ▼
                   ┌────────────────┐
                   │ phoenix/engine │
                   │ (Compositor)   │
                   └────────┬───────┘
                            │
              ┌─────────────┼─────────────┐
              │             │             │
              ▼             ▼             ▼
     ┌────────────────┐ ┌────────────┐ ┌────────────┐
     │  apps/editor   │ │apps/player │ │ apps/render│
     │  (Qt6/QML)     │ │SDL2,Legacy │ │ (Headless) │
     └────────────────┘ └────────────┘ └────────────┘
```

---

## 5. 文件组织规范

### 5.1 头文件 vs 实现文件

| 内容类型 | 放置位置 | 示例 |
|---------|---------|------|
| 类/结构体声明 | `.hpp` | `class Clip { ... };` |
| 简单 getter/setter | `.hpp` (inline) | `int x() const { return x_; }` |
| 模板代码 | `.hpp` 或 `.ipp` | `template<T> class Result` |
| constexpr 函数 | `.hpp` | `constexpr int kMaxTracks = 32;` |
| 复杂成员函数 | `.cpp` | `void Clip::applySpeed(float)` |
| 构造/析构函数 | `.cpp` | `Clip::Clip()` |
| 依赖第三方库的代码 | `.cpp` | FFmpeg 相关 |
| PIMPL Impl 结构 | `.cpp` | `struct Decoder::Impl` |

### 5.2 示例：Clip 模块

```cpp
// ═══════════════════════════════════════════════════════════
// phoenix/model/include/phoenix/model/clip.hpp
// ═══════════════════════════════════════════════════════════
#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/uuid.hpp>
#include <vector>

namespace phoenix::model {

class Clip {
public:
    Clip();
    explicit Clip(UUID mediaItemId);
    ~Clip();
    
    // Getter - inline
    UUID id() const { return id_; }
    UUID mediaItemId() const { return mediaItemId_; }
    Timestamp timelineIn() const { return timelineIn_; }
    Timestamp timelineOut() const { return timelineOut_; }
    
    Duration duration() const { return timelineOut_ - timelineIn_; }
    
    // Setter - 实现在 .cpp
    void setTimelineIn(Timestamp t);
    void setTimelineOut(Timestamp t);
    
    // 复杂方法 - 实现在 .cpp
    Timestamp sourceTimeAt(Timestamp timelineTime) const;
    
private:
    UUID id_;
    UUID mediaItemId_;
    Timestamp timelineIn_ = 0;
    Timestamp timelineOut_ = 0;
    Timestamp sourceIn_ = 0;
    Timestamp sourceOut_ = 0;
    float speed_ = 1.0f;
};

} // namespace phoenix::model
```

```cpp
// ═══════════════════════════════════════════════════════════
// phoenix/model/src/clip.cpp
// ═══════════════════════════════════════════════════════════
#include <phoenix/model/clip.hpp>
#include <stdexcept>

namespace phoenix::model {

Clip::Clip() : id_(UUID::generate()) {}

Clip::Clip(UUID mediaItemId) 
    : id_(UUID::generate())
    , mediaItemId_(mediaItemId) 
{}

Clip::~Clip() = default;

void Clip::setTimelineIn(Timestamp t) {
    if (t >= timelineOut_) {
        throw std::invalid_argument("timelineIn must be < timelineOut");
    }
    timelineIn_ = t;
}

Timestamp Clip::sourceTimeAt(Timestamp timelineTime) const {
    Timestamp offset = timelineTime - timelineIn_;
    return sourceIn_ + static_cast<Timestamp>(offset * speed_);
}

} // namespace phoenix::model
```

### 5.3 Logger 封装 (spdlog 薄封装)

```cpp
// ═══════════════════════════════════════════════════════════
// phoenix/core/include/phoenix/core/logger.hpp
// ═══════════════════════════════════════════════════════════
#pragma once

#include <spdlog/spdlog.h>
#include <memory>
#include <string>

namespace phoenix {

/// Initialize logging system (call once at startup)
void initLogging(const std::string& appName, spdlog::level::level_enum level = spdlog::level::info);

/// Get default logger
std::shared_ptr<spdlog::logger> getLogger();

} // namespace phoenix

// Convenience macros for logging (global scope, simple names)
#define LOG_TRACE(...)    SPDLOG_LOGGER_TRACE(phoenix::getLogger(), __VA_ARGS__)
#define LOG_DEBUG(...)    SPDLOG_LOGGER_DEBUG(phoenix::getLogger(), __VA_ARGS__)
#define LOG_INFO(...)     SPDLOG_LOGGER_INFO(phoenix::getLogger(), __VA_ARGS__)
#define LOG_WARN(...)     SPDLOG_LOGGER_WARN(phoenix::getLogger(), __VA_ARGS__)
#define LOG_ERROR(...)    SPDLOG_LOGGER_ERROR(phoenix::getLogger(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(phoenix::getLogger(), __VA_ARGS__)
```

```cpp
// ═══════════════════════════════════════════════════════════
// phoenix/core/src/logger.cpp
// ═══════════════════════════════════════════════════════════
#include <phoenix/core/logger.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace phoenix {

static std::shared_ptr<spdlog::logger> s_logger;

void initLogging(const std::string& appName, spdlog::level::level_enum level) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(level);
    
    s_logger = std::make_shared<spdlog::logger>(appName, console_sink);
    s_logger->set_level(level);
    s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");
    
    spdlog::set_default_logger(s_logger);
}

std::shared_ptr<spdlog::logger> getLogger() {
    if (!s_logger) {
        initLogging("phoenix");
    }
    return s_logger;
}

} // namespace phoenix
```

**Usage:**
```cpp
#include <phoenix/core/logger.hpp>

void someFunction() {
    LOG_INFO("Processing clip {}", clipId);
    LOG_DEBUG("Frame decoded at {}us", timestamp);
    LOG_ERROR("Failed to open file: {}", path);
}
```

### 5.4 PIMPL 模式 (隐藏 FFmpeg)

```cpp
// phoenix/media/include/phoenix/media/decoder.hpp
#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/media/frame.hpp>
#include <memory>
#include <string>

namespace phoenix::media {

class Decoder {
public:
    Decoder();
    ~Decoder();
    
    Decoder(Decoder&&) noexcept;
    Decoder& operator=(Decoder&&) noexcept;
    
    Result<void, Error> open(const std::string& path);
    Result<VideoFrame, Error> decodeFrame(Timestamp time);
    
private:
    struct Impl;  // FFmpeg 细节隐藏在 .cpp
    std::unique_ptr<Impl> impl_;
};

} // namespace phoenix::media
```

```cpp
// phoenix/media/src/decoder.cpp
#include <phoenix/media/decoder.hpp>

// FFmpeg 只在 .cpp 中引入
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace phoenix::media {

struct Decoder::Impl {
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    
    ~Impl() {
        if (codecCtx) avcodec_free_context(&codecCtx);
        if (formatCtx) avformat_close_input(&formatCtx);
    }
};

Decoder::Decoder() : impl_(std::make_unique<Impl>()) {}
Decoder::~Decoder() = default;
Decoder::Decoder(Decoder&&) noexcept = default;
Decoder& Decoder::operator=(Decoder&&) noexcept = default;

// ... 实现

} // namespace phoenix::media
```

---

## 6. CMake 结构

### 6.1 顶层 CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)
project(Phoenix VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 选项
option(PHOENIX_BUILD_TESTS "Build tests" ON)
option(PHOENIX_BUILD_EDITOR "Build Qt Editor" ON)
option(PHOENIX_BUILD_PLAYER "Build SDL Player" ON)
option(PHOENIX_BUILD_RENDER "Build Headless Renderer" OFF)

# 核心库模块
add_subdirectory(phoenix/core)
add_subdirectory(phoenix/model)
add_subdirectory(phoenix/media)
add_subdirectory(phoenix/effects)
add_subdirectory(phoenix/engine)

# 应用程序
if(PHOENIX_BUILD_EDITOR)
    add_subdirectory(apps/editor)
endif()

if(PHOENIX_BUILD_PLAYER)
    add_subdirectory(apps/player)
endif()

if(PHOENIX_BUILD_RENDER)
    add_subdirectory(apps/render)
endif()
```

### 6.2 模块 CMakeLists.txt 示例

```cmake
# phoenix/model/CMakeLists.txt
add_library(phoenix_model
    src/project.cpp
    src/sequence.cpp
    src/clip.cpp
    src/commands/clip_commands.cpp
)

target_include_directories(phoenix_model
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(phoenix_model
    PUBLIC phoenix_core
    PRIVATE nlohmann_json::nlohmann_json
)

# 测试
if(PHOENIX_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

---

## 7. 核心数据模型

### 7.1 类关系图

```
Project
├── settings: ProjectSettings
│   ├── resolution: Size (1920x1080)
│   ├── frameRate: Rational (30000/1001)
│   └── sampleRate: int (48000)
│
├── mediaBin: MediaBin
│   └── items: map<UUID, MediaItem>
│
└── sequences: vector<Sequence>
    └── Sequence
        ├── videoTracks: vector<Track>
        │   └── clips: vector<Clip>
        └── audioTracks: vector<Track>
            └── clips: vector<Clip>
```

### 7.2 Clip 数据结构

```cpp
struct Clip {
    UUID id;
    UUID mediaItemId;           // 引用 MediaBin 中的素材
    
    // 时间线位置
    Timestamp timelineIn;
    Timestamp timelineOut;
    
    // 源素材位置
    Timestamp sourceIn;
    Timestamp sourceOut;
    
    // 变换
    float speed = 1.0f;
    bool reversed = false;
    float opacity = 1.0f;
    float volume = 1.0f;
    
    // 特效
    vector<EffectInstance> effects;
    
    // 计算属性
    Duration duration() const { return timelineOut - timelineIn; }
};
```

---

## 8. Engine 层设计

### 8.1 Compositor (核心引擎)

```cpp
class Compositor {
public:
    // 请求合成帧 (异步)
    std::future<VideoFrame> requestFrame(
        const Sequence* sequence,
        Timestamp time,
        RenderQuality quality = RenderQuality::Preview
    );
    
    // 渲染单帧 (同步，用于导出)
    VideoFrame renderFrame(
        const Sequence* sequence,
        Timestamp time,
        RenderQuality quality = RenderQuality::Full
    );
    
    void cancelPending();
    
private:
    DecoderPool* decoderPool_;
    FrameCache* frameCache_;
    EffectRegistry* effects_;
};
```

### 8.2 PlaybackEngine

```cpp
class PlaybackEngine {
public:
    void setSequence(const Sequence* sequence);
    
    void play();
    void pause();
    void seek(Timestamp time);
    
    // 信号 (非 Qt)
    Signal<Timestamp> positionChanged;
    Signal<VideoFrame> frameReady;
    
private:
    Compositor* compositor_;
    MasterClock clock_;
    std::thread renderThread_;
};
```

---

## 9. Command Pattern (Undo/Redo)

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
    
    Signal<> changed;
};
```

---

## 10. 实现优先级

### Phase 4.1: 数据模型 (1-2 周)

| 任务 | 文件 | 优先级 |
|------|------|--------|
| 基础类型扩展 | `phoenix/core/uuid.hpp` | P0 |
| 媒体素材 | `phoenix/model/media_item.hpp/.cpp` | P0 |
| 片段 | `phoenix/model/clip.hpp/.cpp` | P0 |
| 轨道 | `phoenix/model/track.hpp/.cpp` | P0 |
| 时间线 | `phoenix/model/sequence.hpp/.cpp` | P0 |
| 项目 | `phoenix/model/project.hpp/.cpp` | P0 |
| JSON 序列化 | `phoenix/model/io/project_io.hpp/.cpp` | P1 |

### Phase 4.2: 引擎层 (2-3 周)

| 任务 | 文件 | 优先级 |
|------|------|--------|
| 解码器池 | `phoenix/media/decoder_pool.hpp/.cpp` | P0 |
| 帧缓存 | `phoenix/engine/frame_cache.hpp/.cpp` | P0 |
| 合成器 | `phoenix/engine/compositor.hpp/.cpp` | P0 |
| 播放引擎 | `phoenix/engine/playback/playback_engine.hpp/.cpp` | P1 |

### Phase 4.3: 命令系统 (1 周)

| 任务 | 文件 | 优先级 |
|------|------|--------|
| 命令基类 | `phoenix/model/commands/command.hpp` | P0 |
| 撤销栈 | `phoenix/model/commands/undo_stack.hpp/.cpp` | P0 |
| 片段命令 | `phoenix/model/commands/clip_commands.hpp/.cpp` | P1 |

### Phase 4.4: Controller 集成 (2 周)

| 任务 | 文件 | 优先级 |
|------|------|--------|
| 项目控制器 | `apps/editor/src/controllers/project_controller.*` | P0 |
| 时间线控制器 | `apps/editor/src/controllers/timeline_controller.*` | P0 |
| 预览控制器 | `apps/editor/src/controllers/preview_controller.*` | P1 |
| 更新 QML | `apps/editor/qml/*.qml` | P1 |

---

## 11. 关键设计决策

| 问题 | 决策 | 原因 |
|------|------|------|
| 目录结构 | `phoenix/` + `apps/` | 清晰分离库和应用，与 namespace 对应 |
| 头文件 vs 实现 | 非模板代码放 .cpp | 减少编译时间，隐藏实现 |
| FFmpeg 依赖 | PIMPL 模式隐藏 | 防止 FFmpeg 头文件污染 |
| Player 代码复用 | 复用 phoenix/media，不复用 Graph | 编辑器用 Pull 模型 |
| Undo 命令位置 | 放在 phoenix/model/commands | 与数据模型紧密相关 |
| 信号槽 | phoenix/core/signals.hpp (非 Qt) | 核心库不依赖 Qt |
| 编解码 | 统一接口支持软硬件 | 运行时自动选择最优方案 |

---

## A2. 软硬件编解码架构设计

### A2.1 设计目标

1. **统一接口** - 对上层透明，隐藏软硬件差异
2. **自动回退** - 硬件不可用时自动降级到软件解码
3. **运行时检测** - 启动时探测可用硬件加速器
4. **帧格式抽象** - 统一处理 CPU/GPU 帧
5. **编码器支持** - 导出时支持硬件编码加速

### A2.2 支持的硬件加速器

| 平台 | 解码 (HW Decode) | 编码 (HW Encode) | FFmpeg 格式 |
|------|-----------------|-----------------|-------------|
| Windows NVIDIA | NVDEC (CUDA) | NVENC | `cuda`, `d3d11va` |
| Windows Intel | QSV | QSV | `qsv`, `d3d11va` |
| Windows AMD | D3D11VA | AMF | `d3d11va` |
| Linux NVIDIA | NVDEC | NVENC | `cuda`, `vaapi` |
| Linux Intel/AMD | VAAPI | VAAPI | `vaapi` |
| macOS | VideoToolbox | VideoToolbox | `videotoolbox` |

### A2.3 Media 模块目录结构

```
phoenix/media/
├── CMakeLists.txt
├── include/phoenix/media/
│   │
│   ├── codec_types.hpp         # 编解码类型枚举
│   │   └── enum class HWAccelType { None, D3D11VA, CUDA, QSV, VAAPI, VideoToolbox }
│   │   └── enum class CodecPreference { Auto, PreferHW, ForceSW, ForceHW }
│   │
│   ├── codec_capability.hpp    # 编解码能力检测 (单例)
│   │   └── static bool isHWAccelAvailable(HWAccelType type)
│   │   └── static std::vector<HWAccelType> availableDecoders()
│   │   └── static std::vector<HWAccelType> availableEncoders()
│   │
│   ├── frame.hpp               # 帧类型 (支持硬件帧)
│   │   └── class VideoFrame { ... isHardware(), transferToCPU() }
│   │   └── class AudioFrame { ... }
│   │
│   ├── decoder.hpp             # 统一解码器接口 (PIMPL)
│   │   └── class Decoder { open(), decodeFrame(), seek(), supportsHW() }
│   │
│   ├── decoder_pool.hpp        # 解码器池
│   │   └── class DecoderPool { acquireDecoder(), releaseDecoder() }
│   │
│   ├── encoder.hpp             # 统一编码器接口 (PIMPL)
│   │   └── class Encoder { open(), encodeFrame(), flush(), supportsHW() }
│   │
│   ├── media_info.hpp          # 媒体信息探测
│   │   └── class MediaInfo { probe(), duration, resolution, codecs }
│   │
│   └── thumbnail.hpp           # 缩略图生成
│
└── src/
    ├── codec_capability.cpp    # 硬件能力检测实现
    ├── decoder.cpp             # Decoder PIMPL 实现
    ├── encoder.cpp             # Encoder PIMPL 实现
    ├── media_info.cpp
    │
    └── ffmpeg/                 # FFmpeg 内部封装 (不暴露)
        ├── ff_utils.hpp            # 现有工具函数
        ├── ff_utils.cpp
        ├── shared_avframe.hpp      # 现有 AVFrame 封装
        │
        ├── ff_hw_device.hpp        # 硬件设备上下文管理
        ├── ff_hw_device.cpp
        │   └── class HWDeviceManager (单例)
        │       ├── initDevice(HWAccelType)
        │       ├── getDeviceContext(HWAccelType) -> AVBufferRef*
        │       └── isDeviceAvailable(HWAccelType) -> bool
        │
        ├── ff_decoder_impl.hpp     # 解码器内部实现
        ├── ff_decoder_impl.cpp
        │   └── struct DecoderImpl
        │       ├── openSoftware(path)
        │       ├── openHardware(path, HWAccelType)
        │       ├── decode() -> SharedAVFrame
        │       └── transferFrame() -> 硬件帧传到 CPU
        │
        ├── ff_encoder_impl.hpp     # 编码器内部实现
        ├── ff_encoder_impl.cpp
        │   └── struct EncoderImpl
        │       ├── openSoftware(settings)
        │       ├── openHardware(settings, HWAccelType)
        │       └── encode(frame) -> packets
        │
        └── ff_frame_transfer.hpp   # 硬件帧传输
        └── ff_frame_transfer.cpp
            └── transferToSoftware(SharedAVFrame& hwFrame) -> SharedAVFrame
            └── transferToHardware(SharedAVFrame& swFrame, HWAccelType) -> SharedAVFrame
```

### A2.4 核心接口设计

#### CodecCapability (能力检测)

```cpp
// phoenix/media/include/phoenix/media/codec_capability.hpp
#pragma once

#include <phoenix/media/codec_types.hpp>
#include <vector>
#include <string>

namespace phoenix::media {

/// Codec capability detection (singleton, thread-safe)
class CodecCapability {
public:
    static CodecCapability& instance();
    
    // Probe all available HW accelerators (call once at startup)
    void probe();
    
    // Decoder capabilities
    bool isDecoderAvailable(HWAccelType type) const;
    std::vector<HWAccelType> availableDecoders() const;
    HWAccelType bestDecoder() const;  // Auto-select best available
    
    // Encoder capabilities
    bool isEncoderAvailable(HWAccelType type) const;
    std::vector<HWAccelType> availableEncoders() const;
    HWAccelType bestEncoder() const;
    
    // Human-readable names
    static std::string hwAccelName(HWAccelType type);
    
private:
    CodecCapability() = default;
    
    std::vector<HWAccelType> m_availableDecoders;
    std::vector<HWAccelType> m_availableEncoders;
    bool m_probed = false;
};

} // namespace phoenix::media
```

#### Decoder (统一解码器)

```cpp
// phoenix/media/include/phoenix/media/decoder.hpp
#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/result.hpp>
#include <phoenix/media/codec_types.hpp>
#include <phoenix/media/frame.hpp>
#include <memory>
#include <string>

namespace phoenix::media {

struct DecoderConfig {
    std::string path;
    CodecPreference preference = CodecPreference::Auto;
    HWAccelType forceHWType = HWAccelType::None;  // Only if ForceHW
    int threadCount = 0;  // 0 = auto
};

class Decoder {
public:
    Decoder();
    ~Decoder();
    
    Decoder(Decoder&&) noexcept;
    Decoder& operator=(Decoder&&) noexcept;
    
    // Open media file
    Result<void, Error> open(const DecoderConfig& config);
    Result<void, Error> open(const std::string& path);  // Auto config
    
    // Decode frame at specific time
    Result<VideoFrame, Error> decodeVideoFrame(Timestamp time);
    Result<AudioFrame, Error> decodeAudioFrame(Timestamp time);
    
    // Sequential decode (for streaming)
    Result<VideoFrame, Error> decodeNextVideoFrame();
    Result<AudioFrame, Error> decodeNextAudioFrame();
    
    // Seek
    Result<void, Error> seek(Timestamp time);
    
    // Properties
    bool isOpen() const;
    bool isHardwareAccelerated() const;
    HWAccelType hwAccelType() const;
    Duration duration() const;
    Size resolution() const;
    Rational frameRate() const;
    
    // Close and release resources
    void close();
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace phoenix::media
```

#### Encoder (统一编码器)

```cpp
// phoenix/media/include/phoenix/media/encoder.hpp
#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/result.hpp>
#include <phoenix/media/codec_types.hpp>
#include <phoenix/media/frame.hpp>
#include <memory>
#include <string>
#include <functional>

namespace phoenix::media {

struct EncoderConfig {
    std::string outputPath;
    
    // Video settings
    Size resolution = {1920, 1080};
    Rational frameRate = {30, 1};
    int videoBitrate = 10'000'000;  // 10 Mbps
    std::string videoCodec = "h264";  // h264, h265, prores, etc.
    
    // Audio settings
    int sampleRate = 48000;
    int audioChannels = 2;
    int audioBitrate = 320'000;  // 320 kbps
    std::string audioCodec = "aac";
    
    // HW acceleration
    CodecPreference preference = CodecPreference::Auto;
    HWAccelType forceHWType = HWAccelType::None;
};

class Encoder {
public:
    Encoder();
    ~Encoder();
    
    Encoder(Encoder&&) noexcept;
    Encoder& operator=(Encoder&&) noexcept;
    
    // Open encoder with config
    Result<void, Error> open(const EncoderConfig& config);
    
    // Encode frames
    Result<void, Error> encodeVideoFrame(const VideoFrame& frame, Timestamp pts);
    Result<void, Error> encodeAudioFrame(const AudioFrame& frame, Timestamp pts);
    
    // Finalize encoding
    Result<void, Error> flush();
    Result<void, Error> close();
    
    // Properties
    bool isOpen() const;
    bool isHardwareAccelerated() const;
    HWAccelType hwAccelType() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace phoenix::media
```

#### VideoFrame (支持硬件帧)

```cpp
// phoenix/media/include/phoenix/media/frame.hpp
#pragma once

#include <phoenix/core/types.hpp>
#include <phoenix/core/result.hpp>
#include <phoenix/media/codec_types.hpp>
#include <memory>
#include <cstdint>

namespace phoenix::media {

class VideoFrame {
public:
    VideoFrame();
    ~VideoFrame();
    
    VideoFrame(const VideoFrame&);
    VideoFrame& operator=(const VideoFrame&);
    VideoFrame(VideoFrame&&) noexcept;
    VideoFrame& operator=(VideoFrame&&) noexcept;
    
    // Properties
    int width() const;
    int height() const;
    PixelFormat format() const;
    Timestamp pts() const;
    
    // Hardware frame info
    bool isHardwareFrame() const;
    HWAccelType hwAccelType() const;
    
    // Transfer between CPU/GPU
    // If already on CPU, returns copy of self
    Result<VideoFrame, Error> transferToCPU() const;
    
    // Access CPU data (only valid if !isHardwareFrame())
    const uint8_t* data(int plane) const;
    int linesize(int plane) const;
    int planeCount() const;
    
    // Check validity
    bool isValid() const;
    explicit operator bool() const { return isValid(); }
    
private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;  // Shared for copy semantics
    
    friend class Decoder;
    friend class Encoder;
};

class AudioFrame {
public:
    AudioFrame();
    ~AudioFrame();
    
    // ... similar interface for audio
    
    int sampleRate() const;
    int channels() const;
    int sampleCount() const;
    SampleFormat format() const;
    Timestamp pts() const;
    
    const uint8_t* data(int channel) const;
    bool isValid() const;
    
private:
    struct Impl;
    std::shared_ptr<Impl> m_impl;
};

} // namespace phoenix::media
```

### A2.5 硬件解码流程

```
┌────────────────────────────────────────────────────────────────────┐
│                        Decoder::open(path)                          │
└─────────────────────────────────┬──────────────────────────────────┘
                                  │
                                  ▼
                    ┌─────────────────────────────┐
                    │  CodecCapability::probe()   │
                    │  检测可用硬件加速器          │
                    └─────────────┬───────────────┘
                                  │
                                  ▼
                    ┌─────────────────────────────┐
                    │  preference == Auto?        │
                    └─────────────┬───────────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
            ┌───────────────┐           ┌───────────────┐
            │ Try HW Decode │           │ ForceSW:      │
            │ (best avail)  │           │ Use Software  │
            └───────┬───────┘           └───────────────┘
                    │
                    ▼
            ┌───────────────────┐
            │ HW init success?  │
            └───────┬───────────┘
                    │
            ┌───────┴───────┐
            ▼               ▼
    ┌───────────────┐ ┌───────────────┐
    │ Use HW Decode │ │ Fallback to   │
    │ (NVDEC/QSV/VT)│ │ SW Decode     │
    └───────────────┘ └───────────────┘
```

### A2.6 帧传输策略

| 场景 | 处理方式 |
|------|---------|
| HW Decode → Preview | 保持 GPU 帧，直接渲染 (OpenGL/D3D11 interop) |
| HW Decode → Export (HW Encode) | 保持 GPU 帧，零拷贝 |
| HW Decode → Export (SW Encode) | 传输到 CPU (av_hwframe_transfer_data) |
| HW Decode → Effect (CPU) | 传输到 CPU |
| HW Decode → Effect (GPU) | 保持 GPU 帧 (Phase 5+) |
| SW Decode → HW Encode | 上传到 GPU (av_hwframe_get_buffer) |

### A2.7 关键设计决策

| 问题 | 决策 | 原因 |
|------|------|------|
| HW 上下文管理 | 单例 `HWDeviceManager` | 避免重复初始化，共享上下文 |
| 接口暴露 | 只暴露 `Decoder`/`Encoder` | PIMPL 隐藏所有 FFmpeg 细节 |
| 帧格式 | `VideoFrame` 内部持有 `SharedAVFrame` | 复用现有引用计数机制 |
| 自动回退 | 默认 `Auto`，可配置 | 灵活性与易用性平衡 |
| GPU 帧传输 | 延迟传输 (lazy) | 仅在需要 CPU 数据时传输 |
| 编码器选择 | 按优先级：NVENC > QSV > VAAPI > SW | 性能优先 |

### A2.8 实现优先级

**Phase 4 (当前):**
- [P0] `CodecCapability` - 能力检测
- [P0] `Decoder` - 软硬件解码
- [P0] `VideoFrame` - 帧抽象 + 传输
- [P1] `DecoderPool` - 解码器池

**Phase 5:**
- [P0] `Encoder` - 软硬件编码
- [P1] GPU 直接渲染 (OpenGL interop)
- [P2] GPU Effect pipeline

---

## 12. 实现策略

**全新实现，不迁移旧代码**。旧的 `src/` 目录将被删除。

| 模块 | 实现方式 | 说明 |
|------|---------|------|
| `phoenix/core/` | ✅ 已完成 | 基础类型、UUID、Logger |
| `phoenix/model/` | 全新实现 | 数据模型 + Undo/Redo |
| `phoenix/media/` | 全新实现 | FFmpeg PIMPL 封装 |
| `phoenix/engine/` | 全新实现 | Compositor + PlaybackEngine |
| `apps/editor/` | 全新实现 | Qt/QML 编辑器 |

> **注意**: `src/` 目录下的旧代码仅作为设计参考，不直接复用。

---

## 13. 执行计划

### 13.1 总体里程碑

```
┌─────────────────────────────────────────────────────────────────────────┐
│  Week 1          Week 2          Week 3          Week 4          Week 5 │
│    │               │               │               │               │    │
│    ▼               ▼               ▼               ▼               ▼    │
│ ┌──────┐       ┌──────┐       ┌──────┐       ┌──────┐       ┌──────┐   │
│ │ M1   │──────▶│ M2   │──────▶│ M3   │──────▶│ M4   │──────▶│ M5   │   │
│ │目录+ │       │数据  │       │媒体+ │       │控制器│       │集成  │   │
│ │Core  │       │模型  │       │引擎  │       │+命令 │       │测试  │   │
│ └──────┘       └──────┘       └──────┘       └──────┘       └──────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```

| 里程碑 | 目标 | 预计时间 | 验收标准 |
|--------|------|----------|----------|
| M1 | 目录结构 + Core 完成 | ✅ 已完成 | 编译通过，Core 库可用 |
| M2 | Model 层完成 | 4-5 天 | 单元测试通过，JSON 序列化工作 |
| M3 | Media + Engine 核心 | 5-7 天 | Compositor 能渲染单帧 |
| M4 | Commands + Controllers | 4-5 天 | Undo/Redo 工作，QML 绑定完成 |
| M5 | 集成测试 + 修复 | 3-4 天 | 完整工作流可用 |

---

### 13.2 详细任务分解

#### M1: 目录结构 + Core 实现 (✅ 已完成)

| ID | 任务 | 文件 | 状态 |
|----|------|------|------|
| M1.1 | 创建 phoenix/ 目录结构 | 目录创建 | ✅ 完成 |
| M1.2 | 创建顶层 CMakeLists.txt | `CMakeLists.txt` | ✅ 完成 |
| M1.3 | 实现 types.hpp | `phoenix/core/include/phoenix/core/types.hpp` | ✅ 完成 |
| M1.4 | 实现 result.hpp | `phoenix/core/include/phoenix/core/result.hpp` | ✅ 完成 |
| M1.5 | 实现 clock.hpp | `phoenix/core/...` | ✅ 完成 |
| M1.6 | 实现 ring_buffer.hpp | `phoenix/core/...` | ✅ 完成 |
| M1.7 | 实现 object_pool.hpp | `phoenix/core/...` | ✅ 完成 |
| M1.8 | 实现 uuid.hpp/cpp | `phoenix/core/...` | ✅ 完成 |
| M1.9 | 实现 logger.hpp/cpp | `phoenix/core/...` | ✅ 完成 |
| M1.10 | Core CMakeLists.txt | `phoenix/core/CMakeLists.txt` | ✅ 完成 |
| M1.11 | 验证编译 | - | ⏳ 待执行 |

**验收标准 M1**:
- [x] `phoenix/core/` 目录结构完整
- [x] `#include <phoenix/core/types.hpp>` 工作
- [ ] `cmake --preset debug` 编译成功

---

#### M2: Model 层 (Day 4-8)

| ID | 任务 | 文件 | 依赖 | 优先级 |
|----|------|------|------|--------|
| M2.1 | MediaItem 结构 | `phoenix/model/.../media_item.hpp/cpp` | M1 | P0 |
| M2.2 | MediaBin 容器 | `phoenix/model/.../media_bin.hpp/cpp` | M2.1 | P0 |
| M2.3 | EffectInstance 结构 | `phoenix/model/.../effect_instance.hpp` | M1 | P0 |
| M2.4 | Clip 结构 | `phoenix/model/.../clip.hpp/cpp` | M2.3 | P0 |
| M2.5 | Track 结构 | `phoenix/model/.../track.hpp/cpp` | M2.4 | P0 |
| M2.6 | Sequence 结构 | `phoenix/model/.../sequence.hpp/cpp` | M2.5 | P0 |
| M2.7 | ProjectSettings | `phoenix/model/.../project_settings.hpp` | M1 | P0 |
| M2.8 | Project 容器 | `phoenix/model/.../project.hpp/cpp` | M2.2, M2.6, M2.7 | P0 |
| M2.9 | JSON 序列化 | `phoenix/model/.../io/project_io.hpp/cpp` | M2.8 | P1 |
| M2.10 | Model CMakeLists.txt | `phoenix/model/CMakeLists.txt` | M2.1-M2.9 | P0 |
| M2.11 | Model 单元测试 | `phoenix/model/tests/...` | M2.10 | P1 |

**验收标准 M2**:
- [ ] 所有 Model 类可实例化
- [ ] Project 可序列化为 JSON 并反序列化
- [ ] 单元测试覆盖 Clip/Track/Sequence 基本操作

---

#### M3: Media + Engine 核心 (Day 5-11)

| ID | 任务 | 文件 | 依赖 | 优先级 |
|----|------|------|------|--------|
| M3.1 | 实现 FFmpeg 内部封装 | `phoenix/media/src/ffmpeg/...` | M1 | P0 |
| M3.2 | 实现 VideoFrame/AudioFrame | `phoenix/media/.../frame.hpp/cpp` | M3.1 | P0 |
| M3.3 | 实现 MediaInfo 探测 | `phoenix/media/.../media_info.hpp/cpp` | M3.1 | P0 |
| M3.4 | 实现 Decoder (PIMPL) | `phoenix/media/.../decoder.hpp/cpp` | M3.2 | P0 |
| M3.5 | 实现 DecoderPool | `phoenix/media/.../decoder_pool.hpp/cpp` | M3.4 | P0 |
| M3.6 | Media CMakeLists.txt | `phoenix/media/CMakeLists.txt` | M3.1-M3.5 | P0 |
| M3.7 | 实现 LRU Cache 模板 | `phoenix/core/.../lru_cache.hpp` | M1 | P0 |
| M3.8 | 实现 FrameCache | `phoenix/engine/.../frame_cache.hpp/cpp` | M3.7 | P0 |
| M3.9 | 实现 Compositor | `phoenix/engine/.../compositor.hpp/cpp` | M3.5, M3.8 | P0 |
| M3.10 | Engine CMakeLists.txt | `phoenix/engine/CMakeLists.txt` | M3.8, M3.9 | P0 |
| M3.11 | 集成测试: 单帧渲染 | 测试代码 | M3.9 | P0 |

**验收标准 M3**:
- [ ] `DecoderPool::getFrame(path, time)` 返回正确帧
- [ ] `Compositor::renderFrame(sequence, time)` 渲染成功
- [ ] FrameCache 缓存命中正常

---

#### M4: Commands + Controllers (Day 16-20)

| ID | 任务 | 文件 | 依赖 | 优先级 |
|----|------|------|------|--------|
| M4.1 | Command 基类 | `phoenix/model/.../commands/command.hpp` | M2 | P0 |
| M4.2 | UndoStack | `phoenix/model/.../commands/undo_stack.hpp/cpp` | M4.1 | P0 |
| M4.3 | AddClipCommand | `phoenix/model/.../commands/clip_commands.hpp/cpp` | M4.2, M2.4 | P0 |
| M4.4 | MoveClipCommand | 同上 | M4.3 | P0 |
| M4.5 | TrimClipCommand | 同上 | M4.3 | P0 |
| M4.6 | DeleteClipCommand | 同上 | M4.3 | P0 |
| M4.7 | PlaybackEngine | `phoenix/engine/.../playback/playback_engine.hpp/cpp` | M3.9 | P0 |
| M4.8 | ProjectController | `apps/editor/.../project_controller.hpp/cpp` | M2.8, M4.2 | P0 |
| M4.9 | TimelineController | `apps/editor/.../timeline_controller.hpp/cpp` | M2.6, M4.3-M4.6 | P0 |
| M4.10 | PreviewController | `apps/editor/.../preview_controller.hpp/cpp` | M4.7 | P0 |
| M4.11 | QML 类型注册 | `apps/editor/src/main.cpp` | M4.8-M4.10 | P0 |

**验收标准 M4**:
- [ ] Undo/Redo 工作正常 (添加 Clip → Undo → Redo)
- [ ] QML 可调用 Controller 方法
- [ ] PreviewController 可播放/暂停

---

#### M5: 集成测试 + 修复 (Day 21-24)

| ID | 任务 | 依赖 | 优先级 |
|----|------|------|--------|
| M5.1 | 更新 Timeline.qml 绑定 | M4.9 | P0 |
| M5.2 | 更新 VideoPreview.qml 绑定 | M4.10 | P0 |
| M5.3 | 更新 Sidebar.qml (MediaBin) | M4.8 | P1 |
| M5.4 | 端到端测试: 导入媒体 | M5.3 | P0 |
| M5.5 | 端到端测试: 添加 Clip 到 Timeline | M5.1 | P0 |
| M5.6 | 端到端测试: 播放预览 | M5.2 | P0 |
| M5.7 | 端到端测试: Undo/Redo | M5.5 | P0 |
| M5.8 | 端到端测试: 保存/加载项目 | M4.8 | P1 |
| M5.9 | Bug 修复 | M5.1-M5.8 | P0 |
| M5.10 | 文档更新 | M5.9 | P2 |

**验收标准 M5**:
- [ ] 完整工作流: 导入 → 添加到时间线 → 播放 → 保存
- [ ] Undo/Redo 无异常
- [ ] 无内存泄漏 (Valgrind/AddressSanitizer)

---

### 13.3 任务依赖图

```
                                    M1 (Core)
                                       │
                    ┌──────────────────┼──────────────────┐
                    │                  │                  │
                    ▼                  ▼                  ▼
              M2 (Model)         M3.1-M3.6           M3.7 (LRU)
                    │            (Media)                  │
                    │                  │                  │
                    │                  ▼                  │
                    │            M3.8 (Cache) ◄───────────┘
                    │                  │
                    ▼                  ▼
              M4.1-M4.6          M3.9 (Compositor)
              (Commands)               │
                    │                  │
                    └────────┬─────────┘
                             │
                             ▼
                    M4.7 (PlaybackEngine)
                             │
                             ▼
                    M4.8-M4.10 (Controllers)
                             │
                             ▼
                    M5 (Integration)
```

---

### 13.4 每日计划

#### Week 1: Model 层 (M2)

| Day | 任务 | 产出 |
|-----|------|------|
| Day 1 | M1.11-M1.12 + M2.1-M2.2 | 删除旧代码 + MediaItem/MediaBin |
| Day 2 | M2.3-M2.5 | EffectInstance/Clip/Track |
| Day 3 | M2.6-M2.8 | Sequence/ProjectSettings/Project |
| Day 4 | M2.9-M2.11 | JSON 序列化 + 单元测试 |

#### Week 2: Media + Engine (M3)

| Day | 任务 | 产出 |
|-----|------|------|
| Day 5 | M3.1-M3.2 | FFmpeg 封装 + Frame 类型 |
| Day 6 | M3.3-M3.4 | MediaInfo + Decoder |
| Day 7 | M3.5-M3.6 | DecoderPool + Media CMake |
| Day 8 | M3.7-M3.8 | LRU Cache + FrameCache |
| Day 9 | M3.9-M3.11 | Compositor + 单帧测试 |

#### Week 3: Commands + Controllers (M4)

| Day | 任务 | 产出 |
|-----|------|------|
| Day 10 | M4.1-M4.2 | Command + UndoStack |
| Day 11 | M4.3-M4.6 | Clip 相关命令 |
| Day 12 | M4.7 | PlaybackEngine |
| Day 13 | M4.8-M4.9 | ProjectController + TimelineController |
| Day 14 | M4.10-M4.11 | PreviewController + QML 注册 |

#### Week 4: 集成 (M5)

| Day | 任务 | 产出 |
|-----|------|------|
| Day 15 | M5.1-M5.3 | QML 绑定更新 |
| Day 16 | M5.4-M5.6 | 端到端测试 |
| Day 17 | M5.7-M5.8 | Undo/Redo + 保存加载测试 |
| Day 18 | M5.9 | Bug 修复 |
| Day 19 | M5.10 + Buffer | 文档 + 缓冲时间 |

---

### 13.5 风险与应对

| 风险 | 可能性 | 影响 | 应对措施 |
|------|--------|------|----------|
| FFmpeg PIMPL 迁移复杂 | 中 | 延期 2 天 | Day 7-8 预留缓冲 |
| Compositor 性能不足 | 中 | 需优化 | 先做功能，后优化 |
| QML 绑定问题 | 低 | 延期 1 天 | 参考现有 VideoController |
| 跨模块依赖错误 | 中 | 编译失败 | 严格按依赖顺序开发 |

---

### 13.6 立即开始的任务

**当前状态**: M1 已完成 80%，需要完成清理和验证。

**Day 1 任务清单**：

1. ✅ `phoenix/core/` 已实现完整
2. ⏳ 删除旧的 `src/` 目录
3. ⏳ 验证 CMake 编译
4. ⏳ 开始 M2: 实现 `MediaItem` 和 `MediaBin`

```bash
# 删除旧代码 (已被 phoenix/core 替代)
rm -rf src/

# 验证编译
cmake --preset debug
cmake --build build/debug --target phoenix_core

# 开始 M2
# - 创建 phoenix/model/CMakeLists.txt
# - 实现 media_item.hpp
```

---

> **注意**: 本设计遵循 YAGNI 原则，Phase 5 的特效系统、关键帧动画等暂不设计。
> 
> **策略变更**: 不迁移旧代码，全新实现。旧 `src/` 仅作设计参考。