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

# FIXME:
# Set GBDK_HOME environment variable
export GBDK_HOME="/c/gbdk/"

# Set debug flag if requested
if [ "$DEBUG" = true ]; then
    echo "Building in debug mode..."
    make GBDK_DEBUG=1 "$@"
else
    echo "Building in release mode..."
    make "$@"
fi

echo "Build successful!"

# Kill any existing BGB process
echo "Closing any existing BGB windows..."
taskkill //F //IM bgb.exe 2>/dev/null || true

echo "Launching BGB..."
"/c/Program Files/bgb/bgb.exe" "obj/Example.gb" & 