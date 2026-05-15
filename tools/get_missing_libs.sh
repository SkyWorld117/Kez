#!/usr/bin/env bash

# Target directory (e.g., your OpenFOAM 'lib' or 'bin' folder)
TARGET_DIR=${1:-.}

# Find all files, use 'file' to ensure they are ELF (binaries or .so)
find "$TARGET_DIR" -type f | while read -r file; do
    if ! file "$file" | grep -qE 'ELF|shared object'; then
        continue
    fi

    # 1. Get DIRECT dependencies (DT_NEEDED)
    # This extracts only the names inside the brackets []
    direct_deps=$(readelf -d "$file" 2>/dev/null | grep "NEEDED" | awk -F'[][]' '{print $2}')
    
    if [ -z "$direct_deps" ]; then continue; fi

    # 2. Get the list of libraries ldd can't find
    # We use -r to force data relocation checks which helps find deeper issues
    missing_in_ldd=$(ldd "$file" 2>/dev/null | grep "not found" | awk '{print $1}')

    # 3. Intersection: Only report if the missing lib is a DIRECT dependency
    found_culprit=false
    for missing in $missing_in_ldd; do
        if echo "$direct_deps" | grep -qx "$missing"; then
            echo "[CULPRIT]: $file"
            echo "    -> Missing Direct Dependency: $missing"
            found_culprit=true
        fi
    done
done