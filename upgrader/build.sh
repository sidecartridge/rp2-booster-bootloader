#!/bin/bash

# Down to main path
cd ..

# Install SDK needed for building
git submodule init
git submodule update --init --recursive

# Set the environment variables of the SDKs
export PICO_SDK_PATH=$PWD/pico-sdk

# Pin the building versions
echo "Pinning the SDK versions..."
cd pico-sdk
#git checkout tags/2.1.0
git checkout tags/2.2.0
cd ..

echo "Pinning the FatFs SDK versions..."
cd fatfs-sdk
#git checkout v3.5.1
#git checkout 6bdb39f96fe8b897aff12bf3416e32515792e318
git checkout tags/v3.6.2
cd ..

# Set the environment variables of the SDKs
export PICO_SDK_PATH=$PWD/pico-sdk
export FATFS_SDK_PATH=$PWD/fatfs-sdk

# Return to upgrader path
cd upgrader

# Check if the third parameter is provided
export RELEASE_TYPE=${3:-""}
echo "Release type: $RELEASE_TYPE"

# Determine the file to use based on RELEASE_TYPE
if [ -z "$RELEASE_TYPE" ] || [ "$RELEASE_TYPE" = "final" ]; then
    VERSION_FILE="version.txt"
else
    VERSION_FILE="version-$RELEASE_TYPE.txt"
fi

# Read the release version from the version.txt file
export RELEASE_VERSION=$(cat "$VERSION_FILE" | tr -d '\r\n ')
echo "Release version: $RELEASE_VERSION"

# Get the release date and time from the current date
export RELEASE_DATE=$(date +"%Y-%m-%d %H:%M:%S")
echo "Release date: $RELEASE_DATE"

# Set the board type to be used for building
# If nothing passed as first argument, use pico
# Use pico instead if pico_w to reduce the footprint
export BOARD_TYPE=${1:-pico}
export PICO_BOARD=$BOARD_TYPE
echo "Board type: $BOARD_TYPE"

# Set the release or debug build type
# If nothing passed as second argument, use release
export BUILD_TYPE=${2:-release}
echo "Build type: $BUILD_TYPE"

# If and only if the build type is debug, enable DEBUG_MODE
if [ "$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')" = "debug" ]; then
    export DEBUG_MODE=1
else
    export DEBUG_MODE=0
fi

# Set the build directory. Delete previous contents if any
echo "Deleting previous build directory"
rm -rf build
mkdir build

# We assume that the last firmware was built for the same board type
# And previously pushed to the repo version

# Build the project
echo "Building the project"
#export PICO_DEOPTIMIZED_DEBUG=1
#echo "PICO_DEOPTIMIZED_DEBUG: $PICO_DEOPTIMIZED_DEBUG"

cd build
cmake ../src -DCMAKE_BUILD_TYPE=$BUILD_TYPE
make -j4 

# Copy the built firmware to the /dist folder
cd ..
mkdir -p dist
echo "Copying the built firmware to the dist folder"
if [ "$BUILD_TYPE" = "release" ]; then
    cp build/upgrader.uf2 dist/upgrader-$BOARD_TYPE.uf2
else
    cp build/upgrader.uf2 dist/upgrader-$BOARD_TYPE-$BUILD_TYPE.uf2
fi

echo "Creating the firmware.h file."
python firmware.py --input=build/upgrader.bin --output=upgrader_firmware.h --array_name=upgrader_firmware --endian_format=big

cp upgrader_firmware.h ../booster/src/include/upgrader_firmware.h
echo "Copied upgrader_firmware.h to booster/src/include/upgrader_firmware.h"
