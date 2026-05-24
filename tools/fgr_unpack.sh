#!/bin/bash
ARCHIVE=$1
DEST=${2:-source}

if [ -z "$ARCHIVE" ]; then
    echo "Usage: $0 <archive> [destination]"
    exit 1
fi

mkdir -p "$DEST"

extract_tar() {
    local tarball=$1
    local dest=$2
    local common_dir
    local strip_components

    # Get the list of all files in the tarball and find the common prefix depth.
    common_dir=$(tar -tf "$tarball" | awk -F/ '
NR==1 {
    split($0, parts, "/")
    len = length(parts) - 1
    for (i=1; i<=len; i++) {
        common[i] = parts[i]
    }
    common_len = len
}
NR>1 {
    split($0, parts, "/")
    len = length(parts) - 1
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

    if [ -z "$common_dir" ] || [ "$common_dir" -eq 0 ]; then
        strip_components=0
    else
        strip_components=$common_dir
    fi

    tar -xf "$tarball" -C "$dest" --strip-components="$strip_components"
}

extract_zip() {
    local zipfile=$1
    local dest=$2
    local common_len
    local common_path
    local tmpdir

    common_len=$(unzip -Z1 "$zipfile" | awk -F/ '
NR==1 {
    split($0, parts, "/")
    len = length(parts) - 1
    for (i=1; i<=len; i++) {
        common[i] = parts[i]
    }
    common_len = len
}
NR>1 {
    split($0, parts, "/")
    len = length(parts) - 1
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

    if [ -z "$common_len" ] || [ "$common_len" -eq 0 ]; then
        unzip -q "$zipfile" -d "$dest"
        return 0
    fi

    common_path=$(unzip -Z1 "$zipfile" | awk -F/ -v n="$common_len" '
NR==1 {
    for (i=1; i<=n; i++) {
        if (i==1) {
            path = $i
        } else {
            path = path "/" $i
        }
    }
    print path
}')

    tmpdir=$(mktemp -d)
    unzip -q "$zipfile" -d "$tmpdir"
    if [ -d "$tmpdir/$common_path" ]; then
        mv "$tmpdir/$common_path"/* "$dest"/
    else
        mv "$tmpdir"/* "$dest"/
    fi
    rm -rf "$tmpdir"
}

case "$ARCHIVE" in
    *.zip)
        extract_zip "$ARCHIVE" "$DEST"
        ;;
    *)
        extract_tar "$ARCHIVE" "$DEST"
        ;;
esac
