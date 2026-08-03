@echo off
REM ============================================================
REM  OracleHelper - Quick Start Script
REM  1. 录制模板 (templates\oracle_1.wav ~ oracle_7.wav)
REM  2. 用 CMake 生成 VS2022 项目
REM  3. 用 VS2022 编译 Release|x64
REM  4. 运行 build\Release\OracleHelper.exe
REM ============================================================

echo.
echo === OracleHelper 快速启动 ===
echo.
echo 步骤 1: 在 templates\ 文件夹中放入录音文件
echo    oracle_1.wav ~ oracle_7.wav (16-bit PCM 或 32-bit float)
echo.
echo 步骤 2: 生成 Visual Studio 项目
echo.

if not exist build mkdir build
cd build

cmake .. -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo.
    echo [错误] CMake 生成失败，请确认已安装:
    echo   - Visual Studio 2022 (含 C++ 桌面开发负载)
    echo   - Windows 10/11 SDK
    echo   - CMake (https://cmake.org/download/)
    pause
    exit /b 1
)

echo.
echo [成功] 项目已生成: build\OracleHelper.sln
echo.
echo 步骤 3: 用 Visual Studio 2022 打开 .sln 文件
echo   选择 Release | x64 配置，按 F7 编译
echo.
echo 步骤 4: 编译完成后运行 OracleHelper.exe
echo.

cd ..
pause
