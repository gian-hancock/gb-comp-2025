#!/bin/bash

# Exit on any error
set -e

# FIXME:
# Set GBDK_HOME environment variable
export GBDK_HOME="/c/gbdk/"

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