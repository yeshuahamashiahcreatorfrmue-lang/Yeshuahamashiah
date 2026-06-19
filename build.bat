@echo off
REM ============================================================
REM  Tsukuru Engine - One-click build for Windows
REM  Double-click this file (or run it) to produce the .exe.
REM  Requirements: CMake + a C++ compiler (Visual Studio 2019/2022
REM  "Desktop development with C++", or MinGW-w64).
REM ============================================================
setlocal
cd /d "%~dp0"

echo ==^> Configuring (Release)...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :error

echo ==^> Building...
cmake --build build --config Release
if errorlevel 1 goto :error

echo.
echo ============================================================
echo  Build complete!
echo  Run:  bin\TsukuruEngine.exe
echo  (Double-click it to launch the editor with the sample game.)
echo ============================================================
echo.
pause
exit /b 0

:error
echo.
echo *** Build failed. Make sure CMake and a C++ compiler are installed. ***
echo.
pause
exit /b 1
