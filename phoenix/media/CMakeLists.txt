# phoenix/media - Media processing library (FFmpeg wrapper)

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

add_library(phoenix_media
    src/frame.cpp
    src/media_info.cpp
    src/decoder.cpp
    src/decoder_pool.cpp
)

target_include_directories(phoenix_media
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(phoenix_media
    PUBLIC
        phoenix::core
        phoenix::model
    PRIVATE
        PkgConfig::LIBAV
)

target_compile_features(phoenix_media PUBLIC cxx_std_20)

# Alias for convenient linking
add_library(phoenix::media ALIAS phoenix_media)
