# 播放器退出机制文档

## 概述

PhoenixEngine 播放器现在具有完善的退出机制，确保在各种情况下都能优雅地关闭和清理资源。

## 退出方式

### 1. 用户交互退出

#### 键盘快捷键
- **ESC键**: 立即退出播放器
- **Q键**: 退出播放器
- **Ctrl+C**: 信号中断（终端强制退出）

#### 窗口操作
- **关闭窗口**: 点击窗口的关闭按钮（X）
- **SDL_QUIT事件**: 系统级退出事件

### 2. 自动退出

#### 正常结束
- **EOF（文件结束）**: 视频播放完毕后自动退出

#### 安全保护
- **超时保护**: 24小时最大运行时间（防止无限循环）
- **异常退出**: 捕获未处理的异常并安全退出

### 3. 信号处理

程序安装了以下信号处理器：
- **SIGINT**: Ctrl+C 中断信号
- **SIGTERM**: 终止信号
- **SIGBREAK** (Windows): Ctrl+Break 信号

## 退出流程

### 标准退出流程

```
1. 检测退出请求（键盘/窗口/信号）
   ↓
2. 设置退出标志
   ↓
3. 停止播放（Player::stop）
   - 停止 Source 节点
   - 停止 Packet Queue
   - 等待 Decode 线程结束
   - 停止 Decoder
   - 停止 Frame Queue
   - 停止 Video Sink
   ↓
4. 关闭播放器（Player::shutdown）
   - 关闭 Renderer
   - 清理所有节点（RAII）
   - 重置资源
   ↓
5. 清理 SDL
   ↓
6. 程序退出
```

## 线程安全

### 原子标志
- `std::atomic<bool> isPlaying_`: 播放状态
- `std::atomic<bool> quitRequested_`: 退出请求标志
- `std::atomic<bool> g_quitRequested`: 全局信号退出标志

### 线程协作
- Decode 线程会定期检查 `isPlaying_` 标志
- 主循环检查 `processEvents()` 返回值
- 信号处理器设置全局标志，主循环响应

## 资源清理顺序

遵循 **下游优先** 原则（与数据流向相反）：

```
VideoSink → FrameQueue → Decoder → PacketQueue → Source → Renderer
```

这确保：
1. 没有节点向已销毁的下游节点推送数据
2. 所有队列有序清空
3. 没有悬空指针或资源泄漏

## 超时保护

### Decode 线程超时
- `InputPin::pop()` 使用 100ms 超时
- 防止线程在队列为空时永久阻塞

### 主循环超时
- 24小时最大运行时间
- 防止意外的无限循环（边界情况保护）

## 日志记录

退出过程中的关键日志：

```
[INFO] Quit key pressed               # 用户按键退出
[INFO] Stopping playback...           # 开始停止播放
[DEBUG] Waiting for decode thread...  # 等待解码线程
[DEBUG] Decode thread joined           # 线程已结束
[INFO] Playback stopped successfully  # 播放停止完成
[INFO] Shutting down player...        # 开始关闭播放器
[DEBUG] Shutting down renderer         # 关闭渲染器
[DEBUG] Cleaning up nodes              # 清理节点
[INFO] Player shutdown complete       # 播放器关闭完成
[INFO] Shutting down SDL...           # 关闭 SDL
[INFO] Shutdown complete. Goodbye!    # 退出完成
```

## 异常处理

### Try-Catch 保护
```cpp
try {
    return phoenix::run(argc, argv);
} catch (const std::exception& e) {
    spdlog::critical("Unhandled exception: {}", e.what());
    return 1;
} catch (...) {
    spdlog::critical("Unknown exception");
    return 1;
}
```

### Result 错误传播
- 使用 `Result<T, E>` 类型进行错误处理
- 不使用异常进行常规错误处理
- 只有致命错误才抛出异常

## 退出代码

| 代码 | 含义 |
|------|------|
| 0    | 正常退出 |
| 1    | 错误退出（初始化失败、文件打开失败等）|
| 2    | 超时退出（运行时间超过24小时）|

## 最佳实践

### 用户侧
1. 使用 **Q** 或 **ESC** 键正常退出
2. 播放完毕会自动退出
3. 必要时使用 **Ctrl+C** 强制退出

### 开发者侧
1. 所有节点析构函数调用 `stop()`
2. 使用 RAII 管理资源
3. 线程退出前检查原子标志
4. 队列操作使用超时避免死锁

## 已知限制

1. **强制终止**: 使用 `kill -9` (Linux) 或任务管理器强制终止会跳过清理
2. **崩溃**: 段错误等致命错误无法保证完全清理
3. **SDL限制**: SDL内部资源由SDL库管理，依赖 `SDL_Quit()`

## 测试建议

```bash
# 1. 正常播放完退出
PhoenixEngine video.mp4

# 2. 按 Q 键退出
PhoenixEngine video.mp4  # 然后按 Q

# 3. Ctrl+C 退出
PhoenixEngine video.mp4  # 然后按 Ctrl+C

# 4. 关闭窗口退出
PhoenixEngine video.mp4  # 然后点击窗口 X
```

## 未来改进

- [ ] 添加退出确认对话框（可选）
- [ ] 保存播放位置以便恢复
- [ ] 统计播放时长并在退出时显示
- [ ] 添加崩溃转储功能
- [ ] 支持后台播放模式

