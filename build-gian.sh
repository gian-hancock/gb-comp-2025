#!/bin/bash

# Exit on any error
set -e

# Parse command line arguments
DEBUG=false
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -d|--debug) DEBUG=true ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

unset LCCFLAGS

# GBDK is now included locally in gbdk/ directory
# No need to set GBDK_HOME - Makefile handles it

# Set debug flag if requested
if [ "$DEBUG" = true ]; then
    echo "Building in debug mode..."
    make GBDK_DEBUG=1 "$@"
else
    echo "Building in release mode..."
    make "$@"
fi

echo "Build successful!"

# Convert WSL path to Windows path for BGB
WIN_ROM_PATH=$(wslpath -w "$(pwd)/obj/Example.gb")

# Kill any existing BGB process
echo "Closing any existing BGB windows..."
cmd.exe /c "taskkill /F /IM bgb.exe" 2>/dev/null || true

echo "Launching BGB with ROM: $WIN_ROM_PATH"
# Use powershell instead of cmd for better UNC path support
powershell.exe -Command "Start-Process 'C:\Program Files\bgb\bgb.exe' -ArgumentList '$WIN_ROM_PATH'" & 