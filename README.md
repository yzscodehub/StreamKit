# StreamKit

一个基于C++的视频流媒体应用程序，使用FFmpeg、CUDA和WebRTC进行实时视频处理。

## 功能特性

- **硬件加速**: 支持NVIDIA NVENC/NVDEC硬件编解码
- **CUDA处理**: GPU加速的视频和音频处理
- **实时同步**: 智能的音视频同步机制
- **高性能**: 优化的内存管理和多线程处理
- **可扩展**: 模块化设计，易于扩展新功能

## 系统要求

- Windows 10/11
- NVIDIA GPU (支持CUDA)
- CUDA Toolkit 11.0+
- Visual Studio 2019/2022
- CMake 3.20+

## 依赖项

项目使用vcpkg管理依赖：

- FFmpeg 6.1
- Boost 1.84.0
- spdlog
- fmt
- nlohmann-json
- websocketpp
- OpenSSL
- zlib
- bzip2

## 构建说明

### 1. 安装vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git E:\WorkSpace\vcpkg
cd E:\WorkSpace\vcpkg
.\bootstrap-vcpkg.bat
```

### 2. 安装依赖

```bash
vcpkg install --triplet=x64-windows
```

### 3. 构建项目

```bash
# 使用构建脚本
build.bat

# 或手动构建
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE="E:/WorkSpace/vcpkg/scripts/buildsystems/vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## 使用方法

```bash
# 处理视频文件
streamkit.exe video.mp4
```

## 项目结构

```
StreamKit/
├── include/                 # 头文件
│   ├── common/             # 通用组件
│   ├── video/              # 视频处理
│   ├── cuda/               # CUDA处理
│   └── webrtc/             # WebRTC相关
├── src/                    # 源文件
│   ├── common/             # 通用组件实现
│   ├── video/              # 视频处理实现
│   ├── cuda/               # CUDA处理实现
│   └── webrtc/             # WebRTC实现
├── CMakeLists.txt          # CMake配置
├── vcpkg.json             # vcpkg依赖配置
└── build.bat              # 构建脚本
```

## 核心组件

### VideoDecoder
- 支持硬件解码 (NVIDIA NVDEC)
- 自动时间戳管理
- 多流支持 (视频+音频)

### VideoEncoder
- 支持硬件编码 (NVIDIA NVENC)
- 低延迟配置
- 自适应码率

### CudaProcessor
- GPU内存管理
- 异步处理
- 内存池优化

### TimestampManager
- 时间戳统一管理
- 音视频同步
- 时间戳校正

## 性能优化

- **内存池**: 减少内存分配开销
- **异步处理**: GPU和CPU并行处理
- **硬件加速**: 充分利用GPU能力
- **智能缓冲**: 自适应缓冲策略

## 开发计划

- [ ] WebRTC服务器实现
- [ ] 前端播放器
- [ ] 网络传输优化
- [ ] 更多编码格式支持
- [ ] 性能监控界面

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request！ 