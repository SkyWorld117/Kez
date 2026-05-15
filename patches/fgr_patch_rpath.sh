#!/usr/bin/env bash

set -Eeuo pipefail

# Input: path to a binary (e.g. .so or executable), and a list of directories to search for shared libraries (concatenated with ':')
# Output: none

# This script fetches the current RPATH of the given binary, adds the specified directories to it, and updates the binary's RPATH accordingly.
if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <binary> <dirs>"
    exit 1
fi

binary="$1"
dirs="$2"

# Fetch the current RPATH of the binary
old_rpath=$(patchelf --print-rpath "$binary")
# Filter out any paths containing '/.tmp/source' and construct the new RPATH
new_rpath=$(echo "$old_rpath" | tr ':' '\n' | grep -v '/\.tmp/source' | tr '\n' ':')
new_rpath="$new_rpath:$dirs"
# Update the binary's RPATH
patchelf --set-rpath "$new_rpath" "$binary"

set +Eeuo pipefail