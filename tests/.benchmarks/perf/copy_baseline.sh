#!/bin/bash

# Script to copy performance baseline files without reading them

SOURCE_FILE="$1"
DEST_FILE="$2"

if [ -z "$SOURCE_FILE" ] || [ -z "$DEST_FILE" ]; then
    echo "Usage: $0 <source_file> <destination_file>"
    exit 1
fi

if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: Source file '$SOURCE_FILE' does not exist"
    exit 1
fi

cp "$SOURCE_FILE" "$DEST_FILE"

if [ $? -eq 0 ]; then
    echo "Successfully copied '$SOURCE_FILE' to '$DEST_FILE'"
else
    echo "Error: Failed to copy file"
    exit 1
fi
