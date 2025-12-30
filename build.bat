@echo off
echo Building StreamKit...

REM 设置vcpkg环境
set VCPKG_ROOT=E:\WorkSpace\vcpkg

REM 创建构建目录
if not exist build mkdir build
cd build

REM 配置CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DCMAKE_BUILD_TYPE=Release

REM 构建项目
cmake --build . --config Release

echo Build completed!
pause 