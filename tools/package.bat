@echo off
rem Package DesktopWidget as a portable zip (Qt + VC runtime self-contained).
rem NOTE: keep this file ASCII-only -- cmd.exe reads .bat as GBK, and a Chinese
rem character whose second byte is 0x5C / 0x22 breaks quoting and line parsing.
setlocal

set "ROOT=%~dp0.."
if not defined QT_DIR set "QT_DIR=D:\Qt\6.8.3\msvc2022_64"
if not defined VS_DIR set "VS_DIR=D:\dev\VS2022BuildTools"
set "QT=%QT_DIR%"
set "VSD=%VS_DIR%"
set "DIST=%ROOT%\dist\DesktopWidget"

if not exist "%ROOT%\build\DesktopWidget.exe" (
  echo [ERROR] not built yet: run tools\build.bat first
  exit /b 1
)

echo [1/5] prepare %DIST%
rem NOTE: do NOT rmdir %DIST% itself -- it can be some shell's current directory and
rem gets locked ("being used by another process"); emptying it always works.
if not exist "%DIST%" mkdir "%DIST%" || exit /b 1
del /f /q /s "%DIST%\*.*" >nul 2>&1
for /d %%d in ("%DIST%\*") do rmdir /s /q "%%d" >nul 2>&1
if exist "%ROOT%\dist\DesktopWidget-*.zip" del /q "%ROOT%\dist\DesktopWidget-*.zip" >nul 2>&1

echo [2/5] copy exe
copy /y "%ROOT%\build\DesktopWidget.exe" "%DIST%\" >nul || exit /b 1

call "%VSD%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

echo [3/5] windeployqt (Qt runtime + platform plugin + QML modules)
rem --no-opengl-sw: Qt6 uses D3D11 by default, skip the 20MB software GL fallback
"%QT%\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --qmldir "%ROOT%\qml" "%DIST%\DesktopWidget.exe" || exit /b 1

echo [4/5] bundle VC143 CRT (no VC++ Redistributable needed)
set "FOUND="
for /d %%D in ("%VSD%\VC\Redist\MSVC\*") do (
  if exist "%%D\x64\Microsoft.VC143.CRT\msvcp140.dll" (
    copy /y "%%D\x64\Microsoft.VC143.CRT\*.dll" "%DIST%\" >nul
    set "FOUND=1"
  )
)
if not defined FOUND echo   [WARN] VC143 CRT not found; target machine needs VC++ runtime

rem windeployqt also drops the 25MB vc_redist installer; we already ship the CRT dlls
if exist "%DIST%\vc_redist.x64.exe" del /q "%DIST%\vc_redist.x64.exe"

copy /y "%ROOT%\tools\dist-README.txt" "%DIST%\README-zh.txt" >nul

echo [5/5] zip
python "%ROOT%\tools\make_zip.py" || exit /b 1

echo.
echo [PACKAGE OK] %ROOT%\dist
dir /b "%ROOT%\dist"
endlocal
