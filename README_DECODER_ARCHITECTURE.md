# StreamKit 解码器架构说明

## 架构概述

StreamKit 采用了面向对象的设计模式，将视频解码功能抽象成一个清晰的层次结构：

```
VideoDecoderBase (抽象基类)
├── SoftwareVideoDecoder (软件解码器)
└── HardwareVideoDecoder (硬件解码器)
    ├── CUDA解码器
    ├── DXVA2解码器
    ├── D3D11VA解码器
    ├── QSV解码器
    └── VAAPI解码器
```

## 核心组件

### 1. VideoDecoderBase (抽象基类)
- **职责**: 定义所有解码器的通用接口和公共逻辑
- **包含**: 文件处理、流管理、帧索引、统计信息等通用功能
- **抽象方法**: `initialize()`, `decodeNextFrame()`, `seekToFrame()`, `close()`

### 2. SoftwareVideoDecoder (软件解码器)
- **职责**: 使用CPU进行视频解码
- **特性**: 
  - 兼容性好，支持所有视频格式
  - 像素格式转换功能
  - 内存占用相对较低
- **适用场景**: 兼容性要求高、不支持硬件加速的环境

### 3. HardwareVideoDecoder (硬件解码器)
- **职责**: 使用GPU进行硬件加速解码
- **特性**:
  - 解码速度快，CPU占用低
  - 支持多种硬件加速标准
  - 硬件帧传输功能
- **适用场景**: 性能要求高、支持硬件加速的环境

### 4. VideoDecoderFactory (工厂类)
- **职责**: 负责创建和管理不同类型的解码器
- **策略模式**: 
  - `PREFER_HARDWARE`: 优先硬件解码，失败回退软件解码
  - `PREFER_SOFTWARE`: 优先软件解码
  - `HARDWARE_ONLY`: 仅使用硬件解码
  - `SOFTWARE_ONLY`: 仅使用软件解码
  - `AUTO_SELECT`: 自动选择最佳解码器

### 5. VideoDecoderManager (管理器)
- **职责**: 提供更高级的解码器管理功能
- **功能**: 解码器切换、全局配置、统计信息管理

## 使用示例

### 基本使用

```cpp
#include "video/decoder_factory.hpp"

// 创建软件解码器
auto software_decoder = VideoDecoderFactory::createSoftwareDecoder();
if (software_decoder && software_decoder->initialize("video.mp4")) {
    auto frame = software_decoder->decodeNextFrame();
    // 处理解码后的帧...
}

// 创建硬件解码器 
auto hardware_decoder = VideoDecoderFactory::createHardwareDecoder(HardwareDecoderType::CUDA);
if (hardware_decoder && hardware_decoder->initialize("video.mp4")) {
    auto frame = hardware_decoder->decodeNextFrame();
    // 处理解码后的帧...
}
```

### 智能选择解码器

```cpp
// 自动选择最佳解码器
auto decoder = VideoDecoderFactory::createDecoder(
    VideoDecoderFactory::CreationStrategy::PREFER_HARDWARE,
    HardwareDecoderType::CUDA
);

if (decoder && decoder->initialize("video.mp4")) {
    std::cout << "使用解码器: " << decoder->getDecoderName() << std::endl;
    
    // 解码循环
    while (!decoder->isEndOfFile()) {
        auto frame = decoder->decodeNextFrame();
        if (frame) {
            // 处理帧...
        }
    }
    
    // 显示统计信息
    auto stats = decoder->getStats();
    std::cout << "解码帧数: " << stats.total_frames_decoded << std::endl;
    std::cout << "平均解码时间: " << stats.average_decode_time_ms << " ms" << std::endl;
    std::cout << "是否硬件加速: " << (stats.is_hardware_accelerated ? "是" : "否") << std::endl;
}
```

### 使用管理器

```cpp
#include "video/decoder_factory.hpp" 

VideoDecoderManager manager;
manager.setGlobalStrategy(VideoDecoderFactory::CreationStrategy::PREFER_HARDWARE);

if (manager.openVideo("video.mp4")) {
    auto decoder = manager.getCurrentDecoder();
    std::cout << "当前解码器: " << decoder->getDecoderName() << std::endl;
    
    // 运行时切换解码器
    if (manager.switchDecoder(DecoderType::SOFTWARE)) {
        std::cout << "已切换到软件解码器" << std::endl;
    }
}
```

## 扩展性

### 添加新的硬件解码器

1. 继承 `HardwareVideoDecoder` 类
2. 实现特定硬件平台的解码逻辑
3. 在工厂类中注册新的解码器类型

```cpp
class MyCustomHardwareDecoder : public HardwareVideoDecoder {
public:
    MyCustomHardwareDecoder() : HardwareVideoDecoder(HardwareDecoderType::CUSTOM) {}
    
    // 实现特定的硬件解码逻辑
    bool initialize(const std::string& file_path) override {
        // 自定义初始化逻辑...
    }
    
    AVFrame* decodeNextFrame() override {
        // 自定义解码逻辑...
    }
};
```

### 添加新的解码策略

在 `VideoDecoderFactory` 中添加新的 `CreationStrategy` 枚举值，并在工厂方法中实现相应逻辑。

## 性能优化

### 硬件解码器优势
- **CUDA解码器**: RTX 4070 Ti 上的 H.264/HEVC 解码性能提升 3-5倍
- **DXVA2/D3D11VA**: Windows 平台的通用硬件加速
- **QSV**: Intel 集显的快速同步视频技术

### 软件解码器优势
- **兼容性**: 支持所有 FFmpeg 支持的视频格式
- **稳定性**: 不依赖硬件驱动，稳定性更好
- **灵活性**: 支持像素格式转换等高级功能

## 调试和监控

### 启用详细日志
```cpp
Logger::initialize("debug.log");
// 设置日志级别为DEBUG可以看到详细的解码过程
```

### 性能监控
```cpp
auto stats = decoder->getStats();
std::cout << "解码性能指标:" << std::endl;
std::cout << "- 总解码帧数: " << stats.total_frames_decoded << std::endl;
std::cout << "- 平均解码时间: " << stats.average_decode_time_ms << " ms" << std::endl;
std::cout << "- 实际解码FPS: " << stats.fps << std::endl;
std::cout << "- 数据处理量: " << stats.total_bytes_processed << " bytes" << std::endl;
```

## 最佳实践

1. **优先使用工厂模式**: 通过 `VideoDecoderFactory` 创建解码器，而不是直接实例化
2. **处理异常情况**: 始终检查解码器初始化和解码操作的返回值
3. **资源管理**: 使用智能指针管理解码器生命周期
4. **性能监控**: 定期检查解码器统计信息，监控性能变化
5. **错误处理**: 实现硬件解码失败时的软件解码回退机制

## 故障排除

### 硬件解码失败
1. 检查显卡驱动是否为最新版本
2. 确认 CUDA Toolkit 已正确安装
3. 检查 FFmpeg 是否编译了硬件解码支持
4. 查看日志文件中的详细错误信息

### 性能问题
1. 使用硬件解码器而不是软件解码器
2. 检查系统资源使用情况
3. 优化帧缓冲区大小
4. 考虑使用多线程解码

这个新的解码器架构为 StreamKit 提供了灵活、可扩展、高性能的视频解码能力，能够充分利用你的 RTX 4070 Ti 显卡的硬件加速功能！ 