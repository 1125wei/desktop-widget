@echo off
rem Build script (Qt6 + cmake/ninja from VS2022 BuildTools).
rem Toolchain locations can be overridden with QT_DIR / VS_DIR.
setlocal

set "ROOT=%~dp0.."
if not defined QT_DIR set "QT_DIR=D:\Qt\6.8.3\msvc2022_64"
if not defined VS_DIR set "VS_DIR=D:\dev\VS2022BuildTools"
set "QT=%QT_DIR%"
set "VSD=%VS_DIR%"

if not exist "%VSD%\VC\Auxiliary\Build\vcvars64.bat" (
  echo [ERROR] vcvars64.bat not found under %VSD%
  echo         set VS_DIR to your Visual Studio / Build Tools folder
  exit /b 1
)
if not exist "%QT%\lib\cmake\Qt6\Qt6Config.cmake" (
  echo [ERROR] Qt6 not found at %QT%
  echo         set QT_DIR to your Qt msvc kit folder
  exit /b 1
)

call "%VSD%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=%VSD%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%QT%\bin;%PATH%"

if not defined CMAKE_BUILD_TYPE set "CMAKE_BUILD_TYPE=Release"
cmake -G Ninja -S "%ROOT%" -B "%ROOT%\build" -DCMAKE_PREFIX_PATH="%QT%" -DCMAKE_BUILD_TYPE=%CMAKE_BUILD_TYPE% || exit /b 1
cmake --build "%ROOT%\build" || exit /b 1

echo [BUILD OK] %ROOT%\build\DesktopWidget.exe
endlocal
