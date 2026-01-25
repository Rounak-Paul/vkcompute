@echo off
REM Build script for VKCompute project (Windows)

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%build
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

echo ========================================
echo   VKCompute Build Script (Windows)
echo ========================================
echo.
echo Build Type: %BUILD_TYPE%
echo Build Dir:  %BUILD_DIR%
echo.

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Configure with CMake
echo Configuring...
cmake -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ..
if errorlevel 1 goto :error

REM Build
echo.
echo Building...
cmake --build . --config %BUILD_TYPE% --parallel
if errorlevel 1 goto :error

echo.
echo ========================================
echo   Build completed successfully!
echo ========================================
echo.
echo Executables are in: %BUILD_DIR%\bin\%BUILD_TYPE%\
echo.
dir /b "%BUILD_DIR%\bin\%BUILD_TYPE%\*.exe" 2>nul || echo (no executables yet)

goto :end

:error
echo.
echo Build FAILED!
exit /b 1

:end
endlocal
