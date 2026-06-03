@echo off
setlocal

if "%~1"=="--help" goto :help
if "%~1"=="-h" goto :help
if "%~1"=="/?" goto :help

set "ARCH="
set "CONFIG="
set "EXTRA_ARGS="

:: Parse all arguments: positional (arch, config) then flags
:parse_args
if "%~1"=="" goto :run
:: If it starts with - or / it's a flag, pass through to PowerShell
set "ARG=%~1"
if "%ARG:~0,1%"=="-" goto :is_flag
if "%ARG:~0,1%"=="/" goto :is_flag
:: Positional arg: first is arch, second is config
if not defined ARCH (
    set "ARCH=%~1"
    shift
    goto :parse_args
)
if not defined CONFIG (
    set "CONFIG=%~1"
    shift
    goto :parse_args
)
:is_flag
:: Flags that take a value: consume both the flag and its argument
set "FLAG=%~1"
if /i "%FLAG%"=="-Platform" goto :flag_with_value
if /i "%FLAG%"=="-Configuration" goto :flag_with_value
if /i "%FLAG%"=="-Compiler" goto :flag_with_value
:: Boolean flag (no value)
set "EXTRA_ARGS=%EXTRA_ARGS% %~1"
shift
goto :parse_args
:flag_with_value
set "EXTRA_ARGS=%EXTRA_ARGS% %~1 %~2"
shift
shift
goto :parse_args

:run
set "PS_ARGS="
if defined ARCH set "PS_ARGS=%PS_ARGS% -Platform %ARCH%"
if defined CONFIG set "PS_ARGS=%PS_ARGS% -Configuration %CONFIG%"

powershell -ExecutionPolicy Bypass -File "%~dp0build-windows.ps1" %PS_ARGS% %EXTRA_ARGS%
exit /b %ERRORLEVEL%

:help
echo.
echo RetroDebugger Windows Build Script
echo.
echo Usage: build-windows.bat [arch] [config] [options]
echo.
echo Arguments:
echo   arch      Target architecture: x64, ARM64 (default: auto-detect)
echo   config    Build configuration: Release, Debug (default: Release)
echo.
echo Options:
echo   -Compiler Clang    Use Clang (ClangCL) compiler (default)
echo   -Compiler MSVC     Use MSVC (v143) compiler
echo   -Clean             Clean all build artifacts and exit (does not build)
echo   -SkipDeps          Skip dependency builds (use prebuilt libs)
echo   -SkipCuda          Skip CUDA plugin build
echo   --help             Show this help message
echo.
echo Examples:
echo   build-windows.bat                              Clang Release x64
echo   build-windows.bat x64 Release -Compiler MSVC   MSVC Release x64
echo   build-windows.bat x64 Release -Clean            Clean rebuild with Clang
echo   build-windows.bat x64 Debug                     Debug build
echo   build-windows.bat x64 Release -SkipDeps         Rebuild only RetroDebugger
echo.
echo Notes:
echo   - CUDA plugin always builds with MSVC regardless of -Compiler setting
echo   - Clang requires "C++ Clang Compiler for Windows" VS component
echo   - Output: platform\Windows\bin\^<arch^>\^<config^>\
echo.
exit /b 0
