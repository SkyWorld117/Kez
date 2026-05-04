#!/bin/bash
TARBALL=$1
DEST=${2:-source}

if [ -z "$TARBALL" ]; then
    echo "Usage: $0 <tarball> [destination]"
    exit 1
fi

mkdir -p "$DEST"

# Get the list of all files in the tarball
# Find how many directory levels are common to all files
# We can use python for a robust commonpath detection if available, 
# but fallback to pure bash with sed/awk.

COMMON_DIR=$(tar -tf "$TARBALL" | awk -F/ '
NR==1 {
    # Initialize common parts from the first path
    split($0, parts, "/")
    if ($0 ~ /\/$/) {
        len = length(parts) - 1
    } else {
        len = length(parts) - 1
    }
    for (i=1; i<=len; i++) {
        common[i] = parts[i]
    }
    common_len = len
}
NR>1 {
    split($0, parts, "/")
    if ($0 ~ /\/$/) {
        len = length(parts) - 1
    } else {
        len = length(parts) - 1
    }
    
    # Compare with current common parts
    new_common_len = 0
    for (i=1; i<=common_len && i<=len; i++) {
        if (common[i] == parts[i]) {
            new_common_len = i
        } else {
            break
        }
    }
    common_len = new_common_len
}
END {
    print common_len
}')

if [ -z "$COMMON_DIR" ] || [ "$COMMON_DIR" -eq 0 ]; then
    STRIP_COMPONENTS=0
else
    STRIP_COMPONENTS=$COMMON_DIR
fi

tar -xf "$TARBALL" -C "$DEST" --strip-components="$STRIP_COMPONENTS"
