#!/bin/bash

# Down to main path
cd ..

# Install SDK needed for building
git submodule init
git submodule update --init --recursive

# Pin the building versions
echo "Pinning the SDK versions..."
cd pico-sdk
#git checkout tags/2.1.0
git checkout tags/2.1.1
cd ..

echo "Pinning the Extras SDK versions..."
cd pico-extras
#git checkout tags/sdk-2.1.0
git checkout tags/sdk-2.1.1
cd ..

echo "Pinning the FatFs SDK versions..."
cd fatfs-sdk
#git checkout v3.5.1
#git checkout 6bdb39f96fe8b897aff12bf3416e32515792e318
git checkout tags/v3.6.2
cd ..

# This is a dirty hack to guarantee that I can use the fatfs-sdk submodule
echo "Patching the fatfs-sdk... to use chmod"
sed -i.bak 's/#define FF_USE_CHMOD[[:space:]]*0/#define FF_USE_CHMOD 1/' fatfs-sdk/src/include/ffconf.h && mv fatfs-sdk/src/include/ffconf.h.bak .

# Set the environment variables of the SDKs
export PICO_SDK_PATH=$PWD/pico-sdk
export FATFS_SDK_PATH=$PWD/fatfs-sdk
export PICO_EXTRAS_PATH=$PWD/pico-extras

# Return to booster path
cd booster

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
# If nothing passed as first argument, use pico_w
export BOARD_TYPE=${1:-pico_w}
export PICO_BOARD=$BOARD_TYPE
echo "Board type: $BOARD_TYPE"

# Set the release or debug build type
# If nothing passed as second argument, use release
export BUILD_TYPE=${2:-release}
echo "Build type: $BUILD_TYPE"

# If the build type is release, set DEBUG_MODE environment variable to 0
# Otherwise set it to 1
if [ "$(echo "$BUILD_TYPE" | tr '[:upper:]' '[:lower:]')" = "release" ]; then
    export DEBUG_MODE=0
else
    export DEBUG_MODE=1
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

# Set more environment variables for the build
export DISPLAY_ATARIST=1
export PICO_FLASH_ASSUME_CORE0_SAFE=1
export BOOSTER_DOWNLOAD_HTTPS=0

echo "DEBUG_MODE: $DEBUG_MODE"
echo "DISPLAY_ATARIST: $DISPLAY_ATARIST"
echo "PICO_FLASH_ASSUME_CORE0_SAFE: $PICO_FLASH_ASSUME_CORE0_SAFE"
echo "BOOSTER_DOWNLOAD_HTTPS: $BOOSTER_DOWNLOAD_HTTPS"
echo "PICO_DEOPTIMIZED_DEBUG: $PICO_DEOPTIMIZED_DEBUG"

cd build
cmake ../src -DCMAKE_BUILD_TYPE=$BUILD_TYPE 
#cmake ../src -DCMAKE_BUILD_TYPE=CustomBuild
#cmake ../src -DCMAKE_BUILD_TYPE=Debug
make -j4 

# Copy the built firmware to the /dist folder
cd ..
mkdir -p dist
echo "Copying the built firmware to the dist folder"
if [ "$BUILD_TYPE" = "release" ]; then
    cp build/booster.uf2 dist/booster-$BOARD_TYPE.uf2
else
    cp build/booster.uf2 dist/booster-$BOARD_TYPE-$BUILD_TYPE.uf2
fi
