cmake_minimum_required(VERSION 3.20)

# 设置CUDA架构 - RTX 4070 Ti使用Ada Lovelace架构 (Compute Capability 8.9)
set(CMAKE_CUDA_ARCHITECTURES "89")

project(StreamKit VERSION 1.0.0 LANGUAGES CXX CUDA)

# 启用CUDA语言支持
enable_language(CUDA)
find_package(CUDAToolkit REQUIRED)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置CUDA标准
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# vcpkg 会自动处理依赖
# FFmpeg 使用 vcpkg 提供的 CMake 配置
find_package(FFMPEG REQUIRED COMPONENTS
    avcodec
    avformat
    avutil
    swscale
)

# 其他库使用 CMake CONFIG（vcpkg 提供）
find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# nlohmann-json 是 header-only 库
find_package(nlohmann_json CONFIG QUIET)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/video
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cuda
    ${CUDAToolkit_INCLUDE_DIRS}
)

# 源文件 - 包含新的解码器架构
set(SOURCES
    "src/main.cpp"
    "src/common/logger.cpp"
    "src/common/error_recovery.cpp"
    "src/video/decoder_base.cpp"
    "src/video/software_decoder.cpp"
    "src/video/hardware_decoder.cpp"
    "src/video/decoder_factory.cpp"
    "src/video/timestamp_manager.cpp"
)

# 如果有 nlohmann/json，添加 config.cpp
if(TARGET nlohmann_json::nlohmann_json)
    list(APPEND SOURCES "src/common/config.cpp")
    message(STATUS "Config system enabled - nlohmann/json found")
else()
    message(WARNING "nlohmann-json not found - Config system will be disabled")
    message(WARNING "Install with: vcpkg install nlohmann-json:x64-windows")
endif()

# 创建主可执行文件
add_executable(streamkit ${SOURCES})

# 创建性能摘要工具
set(SEEK_TEST_SOURCES
    "src/performance_summary.cpp"
    "src/common/logger.cpp"
    "src/common/error_recovery.cpp"
    "src/video/decoder_base.cpp"
    "src/video/software_decoder.cpp"
    "src/video/hardware_decoder.cpp"
    "src/video/decoder_factory.cpp"
    "src/video/timestamp_manager.cpp"
)

if(TARGET nlohmann_json::nlohmann_json)
    list(APPEND SEEK_TEST_SOURCES "src/common/config.cpp")
endif()

add_executable(seek_test ${SEEK_TEST_SOURCES})

# 创建使用示例可执行文件 (仅在有 JSON 库时)
if(TARGET nlohmann_json::nlohmann_json)
    set(EXAMPLE_SOURCES
        "examples/usage_example.cpp"
        "src/common/logger.cpp"
        "src/common/config.cpp"
        "src/common/error_recovery.cpp"
    )
    add_executable(usage_example ${EXAMPLE_SOURCES})
endif()

# 设置CUDA属性
set_property(TARGET streamkit PROPERTY CUDA_RESOLVE_DEVICE_SYMBOLS ON)
set_property(TARGET seek_test PROPERTY CUDA_RESOLVE_DEVICE_SYMBOLS ON)
# usage_example 不使用 CUDA，但保留兼容性

# 链接库 - 使用 vcpkg 提供的 CMake 目标
target_link_libraries(streamkit
    # FFmpeg 库
    ${FFMPEG_LIBRARIES}

    # spdlog 库
    spdlog::spdlog

    # fmt 库
    fmt::fmt

    # CUDA库
    CUDA::cudart
    CUDA::cuda_driver
)

# 添加 FFmpeg 包含目录和库目录
target_include_directories(streamkit PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(streamkit PRIVATE ${FFMPEG_LIBRARY_DIRS})

# 为seek_test链接相同的库
target_link_libraries(seek_test
    # FFmpeg 库
    ${FFMPEG_LIBRARIES}

    # spdlog 库
    spdlog::spdlog

    # fmt 库
    fmt::fmt

    # CUDA库
    CUDA::cudart
    CUDA::cuda_driver
)

# 添加 FFmpeg 包含目录和库目录
target_include_directories(seek_test PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(seek_test PRIVATE ${FFMPEG_LIBRARY_DIRS})

# 为usage_example链接库（需要FFmpeg和CUDA）
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(usage_example
        # FFmpeg 库
        ${FFMPEG_LIBRARIES}

        # spdlog 库
        spdlog::spdlog

        # fmt 库
        fmt::fmt

        # nlohmann_json (header-only)
        nlohmann_json::nlohmann_json

        # CUDA库
        CUDA::cudart
        CUDA::cuda_driver
    )

    # 添加 FFmpeg 和 CUDA 包含目录
    target_include_directories(usage_example PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(usage_example PRIVATE ${FFMPEG_LIBRARY_DIRS})
    target_include_directories(usage_example PRIVATE ${CUDAToolkit_INCLUDE_DIRS})
endif()

# 创建解码器检查工具
add_executable(check_decoders check_decoders.cpp)
target_link_libraries(check_decoders
    ${FFMPEG_LIBRARIES}
)
target_include_directories(check_decoders PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(check_decoders PRIVATE ${FFMPEG_LIBRARY_DIRS})

# 安装配置
install(TARGETS streamkit seek_test
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装头文件
install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)


