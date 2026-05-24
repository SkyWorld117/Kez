#!/usr/bin/env bash

# This script clones a git repository and checks out a specific tag or hash with shallow clone.

set -Eeuo pipefail

url=${1:-}
target=${2:-}
dest=${3:-}

if [[ -z "$url" ]] || [[ -z "$target" ]] || [[ -z "$dest" ]]; then
    echo "Usage: $0 <git_url> <target_tag_or_hash> <destination_directory>"
    exit 1
fi

if git ls-remote --exit-code --heads "$url" "$target" &>/dev/null; then
    # Target is a branch
    git clone --depth 1 --branch "$target" --single-branch "$url" "$dest"
elif git ls-remote --exit-code --tags "$url" "$target" &>/dev/null; then
    # Target is a tag
    git clone --depth 1 --branch "$target" --single-branch "$url" "$dest"
else
    # Target is likely a commit hash
    if [[ ${#target} -lt 40 ]]; then
        # Fall back to clone and checkout if the hash is short (<40 characters)
        ${FROMAGER_HOME}/bin/colored_io/fromager_warning "Hash '$target' is short, falling back to full clone and checkout. Consider using a full 40-character hash for better performance."
        git clone "$url" "$dest"
        (cd "$dest" && git checkout "$target")
    else
        git clone --depth 1 --revision "$target" "$url" "$dest"
    fi
fi

set +Eeuo pipefail