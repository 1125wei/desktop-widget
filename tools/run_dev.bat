@echo off
rem Dev run: QML is loaded from the source folder, so UI edits need no rebuild.
rem Usage (from git-bash, export env vars first to dodge cmd quoting):
rem   export DW_SHOT=1 DW_SHOT_MS=2400 DW_QUIT_MS=3800
rem   MSYS_NO_PATHCONV=1 cmd /c "tools\run_dev.bat"
setlocal
if not defined QT_DIR set "QT_DIR=D:\Qt\6.8.3\msvc2022_64"
set "QT=%QT_DIR%"
set "PATH=%QT%\bin;%PATH%"
if not defined DW_QML_DIR set "DW_QML_DIR=%~dp0..\qml"
"%~dp0..\build\DesktopWidget.exe"
endlocal
