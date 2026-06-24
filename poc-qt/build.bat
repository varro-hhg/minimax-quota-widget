@echo off
REM ---------------------------------------------------------------------
REM MiniMax Quota Widget - build script (Qt 5.13.1 + MinGW 7.3 @ E:\Qt)
REM Run from any cmd window or double-click.
REM ---------------------------------------------------------------------
setlocal

set "QT_DIR=E:\Qt\Qt5.13.1\5.13.1\mingw73_64"
set "MINGW_DIR=E:\Qt\Qt5.13.1\Tools\mingw730_64"
set "PATH=%QT_DIR%\bin;%MINGW_DIR%\bin;%PATH%"

cd /d "%~dp0"

echo === [1/3] qmake ===
qmake MiniMaxQuotaWidget.pro -spec win32-g++ "CONFIG+=release"
if errorlevel 1 goto :fail

echo === [2/3] mingw32-make ===
mingw32-make -j%NUMBER_OF_PROCESSORS%
if errorlevel 1 goto :fail

echo === [3/3] windeployqt ===
if not exist "build\bin\MiniMaxQuotaWidget.exe" (
    echo [ERROR] build\bin\MiniMaxQuotaWidget.exe not found
    goto :fail
)
"%QT_DIR%\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler --no-opengl-sw "build\bin\MiniMaxQuotaWidget.exe"
if errorlevel 1 goto :fail

echo.
echo === Build OK ===
echo Output: build\bin\MiniMaxQuotaWidget.exe
echo Size:
dir /b /s build\bin\*.exe build\bin\*.dll 2>nul | findstr /v "platforms" >nul && (
  for /f "tokens=3" %%a in ('dir build\bin\MiniMaxQuotaWidget.exe ^| findstr "MiniMaxQuotaWidget"') do echo   exe: %%a bytes
)
endlocal
exit /b 0

:fail
echo.
echo === BUILD FAILED ===
endlocal
exit /b 1
