@echo off
REM PhoenixEngine Build Script for Windows

setlocal

REM Configuration
set BUILD_TYPE=Debug
set VCPKG_ROOT=E:\WorkSpace\vcpkg

REM Parse arguments
if "%1"=="debug" set BUILD_TYPE=Debug
if "%1"=="Debug" set BUILD_TYPE=Debug
if "%1"=="release" set BUILD_TYPE=Release
if "%1"=="Release" set BUILD_TYPE=Release
if "%1"=="clean" goto :clean

echo.
echo ============================================
echo  PhoenixEngine Build Script
echo  Build Type: %BUILD_TYPE%
echo ============================================
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo [1/2] Configuring with CMake...
cmake .. ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^
    -DVCPKG_TARGET_TRIPLET=x64-windows

if errorlevel 1 (
    echo CMake configuration failed!
    exit /b 1
)

REM Build
echo.
echo [2/2] Building...
cmake --build . --config %BUILD_TYPE% --parallel

if errorlevel 1 (
    echo Build failed!
    exit /b 1
)

echo.
echo ============================================
echo  Build successful!
echo  Output: build\%BUILD_TYPE%\PhoenixEngine.exe
echo ============================================
echo.

goto :end

:clean
echo Cleaning build directory...
if exist build rmdir /s /q build
echo Done.
goto :end

:end
endlocal

