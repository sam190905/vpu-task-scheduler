@echo off
echo === Building VPU Priority Task Scheduler ===

:: Method 1: Using CMake
cmake -S . -B build -G "MinGW Makefiles" 2>nul
cmake --build build
echo.

echo === Running Scheduler ===
.\build\scheduler.exe

pause
