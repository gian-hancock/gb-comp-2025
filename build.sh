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

# FIXME:
# Set GBDK_HOME environment variable
export GBDK_HOME="/c/gbdk/"

# Set debug flag if requested
if [ "$DEBUG" = true ]; then
    echo "Building in debug mode..."
    export LCCFLAGS="-debug -v -DBGB_DEBUG"
else
    echo "Building in release mode..."
    unset LCCFLAGS
fi

echo "Building project..."
make

if [ $? -eq 0 ]; then
    echo "Build successful!"
    
    # Kill any existing BGB process
    echo "Closing any existing BGB windows..."
    taskkill //F //IM bgb.exe 2>/dev/null || true
    
    echo "Launching BGB..."
    "/c/Program Files/bgb/bgb.exe" "obj/Example.gb" &
else
    echo "Build failed!"
fi

exit $? 