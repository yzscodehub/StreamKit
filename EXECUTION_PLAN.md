# PhoenixEngine 执行计划

> 最后更新: 2026-01-01

## 阶段概览

| 阶段 | 目标 | 状态 | 完成日期 |
|------|------|------|----------|
| Phase 1 | 最小可行播放器 (MVP) | ✅ 已完成 | 2026-01-01 |
| Phase 2 | 音频集成 | ✅ 已完成 | 2026-01-01 |
| Phase 3 | 用户控制 | ✅ 已完成 | 2026-01-01 |
| Phase 4 | 硬件加速 | ⏳ 待开始 | - |

---

## Phase 1: 最小可行播放器 (MVP) ✅

**目标**: 实现视频播放端到端流程

### Step 1.1: 项目结构与基础类型 ✅
- [x] 创建目录结构 (`src/core`, `src/graph`, `src/nodes`, `src/codec`, `src/render`)
- [x] 实现 `core/types.hpp` (Timestamp, Duration, PixelFormat, ErrorCode, PipelineState)
- [x] 实现 `core/result.hpp` (Result<T,E> 错误处理模板)
- [x] 配置 `CMakeLists.txt` 和 `vcpkg.json`

**关键文件**:
- `src/core/types.hpp` - 223 行
- `src/core/result.hpp` - 297 行

### Step 1.2: 媒体帧与 FFmpeg 封装 ✅
- [x] 实现 `codec/ffmpeg/shared_avframe.hpp` (使用 av_frame_ref)
- [x] 实现 `codec/ffmpeg/ff_utils.hpp` (RAII封装、时间戳转换)
- [x] 实现 `core/media_frame.hpp` (Packet, VideoFrame, AudioFrame)

**关键文件**:
- `src/codec/ffmpeg/shared_avframe.hpp` - 200 行
- `src/codec/ffmpeg/ff_utils.hpp` - 367 行
- `src/core/media_frame.hpp` - 375 行

### Step 1.3: Graph 系统核心 ✅
- [x] 实现 `graph/concepts.hpp` (C++20 Concepts: Transferable)
- [x] 实现 `graph/pin.hpp` (InputPin 背压队列, OutputPin)
  - [x] flush() 清空队列
  - [x] stop() 唤醒阻塞线程
  - [x] PopResult 枚举
- [x] 实现 `graph/node.hpp` (INode, SourceNode, ProcessorNode, SinkNode)
- [x] 实现 `graph/async_queue.hpp` (AsyncQueueNode 线程边界)
  - [x] 析构函数调用 stop()

**关键文件**:
- `src/graph/concepts.hpp` - ~80 行
- `src/graph/pin.hpp` - 346 行
- `src/graph/node.hpp` - ~200 行
- `src/graph/async_queue.hpp` - ~180 行

### Step 1.4: MasterClock 与 ObjectPool ✅
- [x] 实现 `core/clock.hpp` (SeqLock 无锁时钟)
  - [x] update() 单写者
  - [x] now() 多读者无锁
  - [x] useWallClock() 无音频回退
  - [x] pause/resume/seek
- [x] 实现 `core/object_pool.hpp` (线程安全对象池)

**关键文件**:
- `src/core/clock.hpp` - 260 行
- `src/core/object_pool.hpp` - 286 行

### Step 1.5: 业务节点实现 ✅
- [x] 实现 `nodes/source_node.hpp` (FileSourceNode)
  - [x] av_read_frame 循环
  - [x] 时间基转换 (av_rescale_q)
  - [x] pause/resume/seekTo
  - [x] Serial 标记
- [x] 实现 `nodes/decode_node.hpp` (FFmpegDecodeNode)
  - [x] process() 1-to-N 输出
  - [x] EOF drain (avcodec_send_packet(NULL))
  - [x] flushCodec() (avcodec_flush_buffers)
  - [x] 迭代限制 (max 100)
  - [x] 连续错误计数
- [x] 实现 `nodes/video_sink_node.hpp` (VideoSinkNode)
  - [x] A/V sync (500ms/100ms/40ms阈值)
  - [x] Serial 过滤
  - [x] Pre-roll 首帧通知

**关键文件**:
- `src/nodes/source_node.hpp` - 406 行
- `src/nodes/decode_node.hpp` - 324 行
- `src/nodes/video_sink_node.hpp` - 310 行

### Step 1.6: 渲染层 ✅
- [x] 实现 `render/renderer.hpp` (IRenderer, IFrameUploader 接口)
- [x] 实现 `render/sdl_renderer.hpp` (SDL2 实现)
  - [x] YUV420P/NV12 texture 上传
  - [x] 宽高比保持

**关键文件**:
- `src/render/renderer.hpp` - ~100 行
- `src/render/sdl_renderer.hpp` - 360 行

### Step 1.7: Pipeline 管理器 ✅
- [x] 实现 `graph/pipeline.hpp`
  - [x] connect<T>() 类型安全连接
  - [x] start() 先启动下游
  - [x] stop() 先停 InputPin
  - [x] seek() 完整流程
  - [x] PipelineState 状态机

**关键文件**:
- `src/graph/pipeline.hpp` - 453 行

### Step 1.8: MVP 集成 ✅
- [x] 更新 `main.cpp` (Player 类)
  - [x] 创建完整管道
  - [x] 暂停/恢复 (Space)
  - [x] Seek (←/→ ±5秒)
  - [x] EOF 处理
  - [x] 窗口事件处理

**关键文件**:
- `src/main.cpp` - 474 行

---

## Phase 2: 音频集成 ✅

**目标**: 添加音频播放和 A/V 同步

### Step 2.1: 音频帧与重采样 ✅
- [x] 扩展 `media_frame.hpp` (AudioFrame 已存在，已验证)
- [x] 实现 `LockFreeRingBuffer` (128KB, ~680ms 容量)
- [x] 实现 `nodes/audio_sink_node.hpp`
  - [x] SwrContext 格式转换 (任意格式 -> S16 stereo 48kHz)
  - [x] AVChannelLayout 新 API
  - [x] SDL 音频回调驱动 MasterClock

**关键文件**:
- `src/core/ring_buffer.hpp` - 300+ 行
- `src/nodes/audio_sink_node.hpp` - 400+ 行
- `src/nodes/audio_decode_node.hpp` - 250+ 行

**约束满足**:
- RingBuffer: 131072 bytes (128KB, power-of-2)
- 使用 `swr_alloc_set_opts2()` (FFmpeg 5.0+ API)
- 使用 `AVChannelLayout` 结构体

### Step 2.2: 多流分离 ✅
- [x] 扩展 `FileSourceNode` 音频输出 (已预留 audioOutput，已启用)
- [x] 实现 `AudioDecodeNode` (类似 FFmpegDecodeNode)
- [x] 更新 Pipeline 构建
  - [x] 视频队列: 50 packets
  - [x] 音频队列: 1000 packets

### Step 2.3: A/V 同步调试 ✅
- [x] SDL 回调中更新 Clock
  - [x] 暂停时只填充静音，不更新 Clock
- [x] 无音频场景处理
  - [x] hasAudioSource() 检查
  - [x] useWallClock() 回退
- [x] 添加同步调试日志
- [x] Pre-roll: 等待音视频都 ready 后启动 Clock

---

## Phase 3: 用户控制 ✅

**目标**: 实现 Seek、Pause 和健壮性

### Step 3.1: Seek 机制 ✅
- [x] Serial Number 机制
  - [x] Pipeline 维护 seekSerial_
  - [x] Packet/VideoFrame 携带 serial
  - [x] Sink 过滤旧 serial
- [x] Pipeline::seek() 流程
  - [x] pause source
  - [x] flush queues
  - [x] avcodec_flush_buffers
  - [x] reset clock
  - [x] resume
- [x] 音频 RingBuffer 清空 (audioSink_->flush() 调用 ringBuffer_.clear())

### Step 3.2: Pause/Resume 与 Pre-roll ✅
- [x] Pipeline pause/resume
- [x] 完整 Pre-roll 状态机
  - [x] BufferingState 枚举 (Idle, WaitingVideo, WaitingAudio, WaitingBoth, Ready, Timeout)
  - [x] PipelineState::Buffering 状态
  - [x] 500ms 超时回退 Wall Clock (kPrerollTimeoutMs)
  - [x] checkPrerollTimeout() 定时检查
  - [x] completePreroll() 启动时钟

### Step 3.3: 错误恢复 ✅
- [x] DecoderNode 连续错误计数
- [x] Pipeline 优雅降级
  - [x] AudioFrame/VideoFrame::isError() 检测
  - [x] SinkErrorCallback 错误事件到 UI
  - [x] PlayerEvent 枚举 (Ready, Playing, Paused, Error, Warning 等)
  - [x] EventCallback 回调机制
  - [x] consecutiveErrors_ 跟踪和阈值处理

**关键文件更新**:
- `src/core/types.hpp` - 添加 BufferingState, PlayerEvent 枚举
- `src/core/media_frame.hpp` - 添加 AudioFrame::isError()
- `src/nodes/video_sink_node.hpp` - 添加 SinkErrorCallback
- `src/nodes/audio_sink_node.hpp` - 添加错误回调和跟踪
- `src/main.cpp` - 完整状态机实现

---

## Phase 4: 硬件加速 ⏳

**目标**: 提升解码性能

### Step 4.1: 硬件上下文管理
- [ ] 实现 `HardwareContext` (shared_ptr 封装)
  - [ ] 持有 AVBufferRef* (hw_device_ctx)
  - [ ] 防止 Decoder 销毁后 Frame 失效
- [ ] 扩展 `VideoFrame::HardwareFrame`
  - [ ] 持有 shared_ptr<HardwareContext>
- [ ] Pipeline 销毁顺序
  - [ ] stop source -> drain -> stop sinks -> destroy decoder

### Step 4.2: D3D11/CUDA 解码器
- [ ] 实现 `HardwareDecodeNode`
  - [ ] av_hwdevice_ctx_create (D3D11/CUDA)
  - [ ] 硬件格式协商
- [ ] 实现 `D3D11Uploader`
  - [ ] Zero-copy 纹理共享
- [ ] `AdaptiveDecodeNode`
  - [ ] 硬件失败自动回退软件
  - [ ] hwFailed_ 状态记录

---

## 文件清单

### Core 层 (`src/core/`)
| 文件 | 说明 | 状态 |
|------|------|------|
| `types.hpp` | 基础类型定义 | ✅ |
| `result.hpp` | Result<T,E> 错误处理 | ✅ |
| `media_frame.hpp` | Packet, VideoFrame, AudioFrame | ✅ |
| `object_pool.hpp` | 线程安全对象池 | ✅ |
| `clock.hpp` | SeqLock 无锁时钟 | ✅ |
| `ring_buffer.hpp` | 无锁环形缓冲区 | ✅ |

### Graph 层 (`src/graph/`)
| 文件 | 说明 | 状态 |
|------|------|------|
| `concepts.hpp` | C++20 Concepts | ✅ |
| `pin.hpp` | InputPin, OutputPin | ✅ |
| `node.hpp` | 节点基类 | ✅ |
| `async_queue.hpp` | 异步队列节点 | ✅ |
| `pipeline.hpp` | Pipeline 管理器 | ✅ |

### Codec 层 (`src/codec/ffmpeg/`)
| 文件 | 说明 | 状态 |
|------|------|------|
| `shared_avframe.hpp` | AVFrame RAII 封装 | ✅ |
| `ff_utils.hpp` | FFmpeg 工具函数 | ✅ |

### Nodes 层 (`src/nodes/`)
| 文件 | 说明 | 状态 |
|------|------|------|
| `source_node.hpp` | 文件解复用 | ✅ |
| `decode_node.hpp` | 视频解码 | ✅ |
| `video_sink_node.hpp` | 视频渲染 | ✅ |
| `audio_decode_node.hpp` | 音频解码 | ✅ |
| `audio_sink_node.hpp` | 音频渲染 | ✅ |

### Render 层 (`src/render/`)
| 文件 | 说明 | 状态 |
|------|------|------|
| `renderer.hpp` | 渲染器接口 | ✅ |
| `sdl_renderer.hpp` | SDL2 实现 | ✅ |

---

## 运行说明

### 编译
```powershell
cd E:\WorkSpace\StreamKit
cmake --build build --config Debug
```

### 运行
```powershell
cd build\Debug
.\PhoenixEngine.exe <video_file.mp4>
```

### 控制键
| 按键 | 功能 |
|------|------|
| `Space` | 暂停/恢复 |
| `←` | 后退 5 秒 |
| `→` | 前进 5 秒 |
| `ESC` | 退出 |

---

## 设计决策摘要

| 问题 | 解决方案 |
|------|----------|
| 1-to-N 解码 | `process()` 返回 void，循环 `emit()` |
| AVFrame 生命周期 | `SharedAVFrame` 使用 `av_frame_ref` |
| 背压控制 | `InputPin` 有界队列阻塞生产者 |
| 时钟竞争 | SeqLock 无锁读 |
| 管道死锁 | 先停 InputPin，Poison Pill 模式 |
| Seek 刷新 | flush queues + `avcodec_flush_buffers` |
| EOF 排空 | `avcodec_send_packet(NULL)` + 循环 receive |
| 启动同步 | Pre-roll 状态机 |
| 线程安全 | `AsyncQueueNode` 析构必须调用 `stop()` |
| A/V 漂移 | 自适应同步: >100ms 丢帧, >500ms 等待 |
| Seek 原子性 | Serial Number 过滤旧帧 |
| 时间精度 | int64_t 微秒 + av_rescale_q |

---

## 版本历史

| 版本 | 日期 | 变更 |
|------|------|------|
| 0.1.0 | 2026-01-01 | Phase 1 MVP 完成 |
| 0.2.0 | 2026-01-01 | Phase 2 音频集成完成 |
| 0.3.0 | 2026-01-01 | Phase 3 用户控制完成 (Pre-roll 状态机、错误恢复、事件系统) |

