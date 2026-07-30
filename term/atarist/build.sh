#!/bin/bash

# Fail fast. Without this a failed cmake, make, link or missing tool was simply
# stepped over: the script carried on, copied whatever binary a previous build
# had left behind, and exited 0. A root build could therefore package stale
# firmware -- or none at all -- with nothing to show anything had gone wrong.
# `-u` is deliberately not set; several scripts test unset positional args.
set -Eeo pipefail
trap 'echo "ERROR: ${BASH_SOURCE[0]}: failed at line ${LINENO}" >&2' ERR

# Ensure an argument is provided
if [ -z "$1" ]; then
    echo "Usage: $0 <working_folder> all|release"
    exit 1
fi

if [ -z "$2" ]; then
    echo "Usage: $0 <working_folder> all|release"
    exit 1
fi

working_folder=$1
build_type=$2

# stcmd runs `docker run -it`, which fails outright when stdin is not a
# terminal: "cannot attach stdin to a TTY-enabled container". That is every
# non-interactive run -- CI, a script, an agent -- and none of them need a TTY,
# since this build only invokes make, cp, stat and truncate. STCMD_NO_TTY=1
# drops the flag. Interactive runs are left alone.
if [ ! -t 0 ]; then
    export STCMD_NO_TTY=1
fi

# ST_WORKING_FOLDER=$working_folder/configurator stcmd make $build_type
ST_WORKING_FOLDER=$working_folder stcmd make $build_type

#filename_tos="./dist/SIDECART.TOS"

# Copy the SIDECART.TOS file for testing purposes
#ST_WORKING_FOLDER=$working_folder stcmd cp ./configurator/dist/SIDECART.TOS $filename_tos

filename="./dist/FIRMWARE.IMG"

# Copy the BOOT.BIN file to a ROM size file for testing
ST_WORKING_FOLDER=$working_folder stcmd cp ./dist/BOOT.BIN $filename

# Determine the file size accordingly
filesize=$(ST_WORKING_FOLDER=$working_folder stcmd stat -c %s "$filename")

# Size for 64Kbytes in bytes
#targetsize=$((64 * 1024))
targetsize=2048

# Check if the file is larger than 64Kbytes
if [ "$filesize" -gt "$targetsize" ]; then
    echo "The file is already larger than 64Kbytes."
    exit 2
fi

# Resize the file to 64Kbytes
ST_WORKING_FOLDER=$working_folder stcmd truncate -s $targetsize $filename

if [ $? -ne 0 ]; then
    echo "Failed to resize the file."
    exit 3
fi

echo "File has been resized."

echo "Creating the firmware.h file."
python firmware.py --input=dist/FIRMWARE.IMG --output=term_firmware.h --array_name=term_firmware

cp term_firmware.h ../../booster/src/include/term_firmware.h
echo "Copied term_firmware.h to booster/src/include/term_firmware.h"