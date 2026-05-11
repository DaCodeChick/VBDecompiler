@echo off
REM Build and test script for VB Decompiler (Windows)

echo ================================================
echo   VB Decompiler - Build and Test
echo ================================================
echo.

REM Check prerequisites
echo [1/5] Checking prerequisites...
where zig >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo X Error: Zig compiler not found
    echo    Install from: https://ziglang.org/download/
    exit /b 1
)

for /f "tokens=*" %%i in ('zig version') do set ZIG_VERSION=%%i
echo + Zig version: %ZIG_VERSION%

where cmake >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo ! Warning: CMake not found (required for full build with GUI)
) else (
    for /f "tokens=*" %%i in ('cmake --version ^| findstr /C:"cmake version"') do set CMAKE_VERSION=%%i
    echo + %CMAKE_VERSION%
)

echo.

REM Build core library
echo [2/5] Building Zig core library...
cd core
zig build -Doptimize=ReleaseSafe
if %ERRORLEVEL% NEQ 0 (
    echo X Build failed
    exit /b 1
)
cd ..
echo + Core library built successfully
echo.

REM Run tests
echo [3/5] Running unit tests...
cd core
zig build test
if %ERRORLEVEL% NEQ 0 (
    echo ! Some tests failed
) else (
    echo + All tests passed
)
cd ..
echo.

REM Verify artifacts
echo [4/5] Verifying build artifacts...
if exist "core\zig-out\lib\vbdecomp.dll" (
    echo + Shared library: vbdecomp.dll
) else (
    echo X Shared library not found
    exit /b 1
)

if exist "core\zig-out\bin\vbdecomp.exe" (
    echo + CLI tool: vbdecomp.exe
) else (
    echo X CLI tool not found
    exit /b 1
)
echo.

REM Test CLI
echo [5/5] Testing CLI tool...
core\zig-out\bin\vbdecomp.exe | findstr "VBDecompiler CLI" >nul
if %ERRORLEVEL% EQU 0 (
    echo + CLI tool works
) else (
    echo X CLI tool failed
    exit /b 1
)
echo.

echo ================================================
echo   + Build completed successfully!
echo ================================================
echo.
echo Artifacts:
echo   - Library: core\zig-out\lib\
echo   - CLI:     core\zig-out\bin\vbdecomp.exe
echo.
echo Run CLI: core\zig-out\bin\vbdecomp.exe
