#!/bin/bash
# Build RetroDebugger on Windows from Git Bash / MSYS2 / WSL.
# Thin wrapper around build-windows.ps1 — pass the same flags.
#
# Usage: ./build-windows.sh [arch] [config] [options]
#
# Arguments:
#   arch      Target architecture: x64, ARM64 (default: auto-detect)
#   config    Build configuration: Release, Debug (default: Release)
#
# Options:
#   -Compiler Clang    Use Clang (ClangCL) compiler (default)
#   -Compiler MSVC     Use MSVC (v143) compiler
#   -Clean             Clean all build artifacts and exit (does not build)
#   -SkipDeps          Skip dependency builds (use prebuilt libs)
#   -SkipCuda          Skip CUDA plugin build
#   --help, -h         Show this help message
#
# Examples:
#   ./build-windows.sh                                Clang Release x64
#   ./build-windows.sh x64 Release -Compiler MSVC     MSVC Release x64
#   ./build-windows.sh x64 Release -Clean             Clean rebuild with Clang
#   ./build-windows.sh x64 Debug                      Debug build
#   ./build-windows.sh x64 Release -SkipDeps          Rebuild only RetroDebugger

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "$1" == "--help" || "$1" == "-h" ]]; then
    # Print the header comment block as help text
    sed -n '2,/^$/{ s/^# \?//; p; }' "${BASH_SOURCE[0]}"
    exit 0
fi

# Find powershell — prefer pwsh (PowerShell 7+), fall back to powershell.exe
if command -v pwsh.exe &>/dev/null; then
    PS=pwsh.exe
elif command -v powershell.exe &>/dev/null; then
    PS=powershell.exe
elif command -v pwsh &>/dev/null; then
    PS=pwsh
elif command -v powershell &>/dev/null; then
    PS=powershell
else
    echo "Error: PowerShell not found. Install PowerShell or use build-windows.bat instead." >&2
    exit 1
fi

# Convert script path to Windows-style if running under MSYS/Git Bash
PS1_SCRIPT="$SCRIPT_DIR/build-windows.ps1"
if command -v cygpath &>/dev/null; then
    PS1_SCRIPT="$(cygpath -w "$PS1_SCRIPT")"
fi

# Parse positional args (arch, config) and pass everything to PowerShell
ARCH=""
CONFIG=""
EXTRA_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -*)
            # Flag argument — check if it takes a value
            case "$1" in
                -Platform|-Configuration|-Compiler)
                    EXTRA_ARGS+=("$1" "$2")
                    shift 2
                    ;;
                *)
                    EXTRA_ARGS+=("$1")
                    shift
                    ;;
            esac
            ;;
        *)
            # Positional: first is arch, second is config
            if [[ -z "$ARCH" ]]; then
                ARCH="$1"
            elif [[ -z "$CONFIG" ]]; then
                CONFIG="$1"
            fi
            shift
            ;;
    esac
done

PS_ARGS=()
[[ -n "$ARCH" ]]   && PS_ARGS+=("-Platform" "$ARCH")
[[ -n "$CONFIG" ]] && PS_ARGS+=("-Configuration" "$CONFIG")

exec "$PS" -ExecutionPolicy Bypass -File "$PS1_SCRIPT" "${PS_ARGS[@]}" "${EXTRA_ARGS[@]}"
