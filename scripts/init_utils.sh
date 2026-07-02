#!/usr/bin/env bash

check_installation() {
    local pkg="$1"
    if $(yq -r ".state | has(\"${pkg}\")" "${!_SYSTEM}/state.yaml"); then
        return 1
    else
        return 0
    fi
}

fetch_web() {
    local pkg="$1"
    local version="$2"
    local url="$3"
    local ext="${4:-tar.xz}"

    if ! check_installation "${pkg}"; then
        return 0
    fi

    local filename

    if [[ -n "${version}" ]] && [[ "${version}" != "latest" ]]; then
        filename=$(curl -sSL "$url" \
            | grep -oE "${pkg}-${version}\.${ext}" \
            | sort -Vu \
            | tail -n1)
    else
        filename=$(curl -sSL "$url" \
            | grep -oE "${pkg}-[0-9]+\.[0-9]+(\.[0-9]+)?\.${ext}" \
            | sort -Vu \
            | tail -n1)
    fi

    if [[ -z "$filename" ]]; then
        echo >&2 "No files found for ${pkg} with extension .${ext} at $url"
        return 1
    fi

    wget --quiet --show-progress --output-document="${!_SYSTEM_TMP}/${filename}" "${url}${filename}"

    # Check if the file was downloaded successfully
    if [ ! -f "${!_SYSTEM_TMP}/${filename}" ]; then
        echo >&2 "Failed to download ${filename} from ${url}"
        return 1
    fi

    if [ ${ext} = "tar.xz" ]; then
        tar -xf "${!_SYSTEM_TMP}/${filename}"
    elif [ ${ext} = "tar.gz" ]; then
        tar -xzf "${!_SYSTEM_TMP}/${filename}"
    elif [ ${ext} = "tar.bz2" ]; then
        tar -xjf "${!_SYSTEM_TMP}/${filename}"
    else
        echo >&2 "Unsupported file extension: ${ext}"
        return 1
    fi

    local version="${filename#${pkg}-}"
    version="${version%.${ext}}"

    echo "${filename%.tar.*}::${version}"
}

fetch_gnu() {
    local pkg="$1"
    local version="${2}"
    local ext="${3:-tar.xz}"
    # GNU official
    local url="https://ftp.gnu.org/gnu/${pkg}/"
    # GNU mirror (usually much faster)
    # local url="https://ftpmirror.gnu.org/gnu/${pkg}/"

    echo $(fetch_web "${pkg}" "${version}" "${url}" "${ext}")
}

fetch_isl() {
    local url="https://libisl.sourceforge.io/"
    local version="${1}"
    local ext="${2:-tar.xz}"

    echo $(fetch_web "isl" "${version}" "${url}" "${ext}")
}

fetch_rust() {
    local url=https://forge.rust-lang.org/infra/archive-stable-version-installers.html
    local version="${1}"

    if ! check_installation "rust"; then
        return 0
    fi

    if [[ -n "${version}" ]] && [[ "${version}" == "latest" ]]; then
        version=$(curl -sSL "${url}" | grep -oE "Stable \([0-9]+\.[0-9]+(\.[0-9]+)?\)" | sort -Vu | tail -n 1)
        version="${version#Stable (}"
        version="${version%)}"
    fi

    local suffix
    if [ "${!_ARCH}" == "x86_64" ]; then
        suffix="x86_64-unknown-linux-gnu"
    elif [ "${!_ARCH}" == "arm64" ]; then
        suffix="aarch64-unknown-linux-gnu"
    else
        echo >&2 "Unsupported architecture: ${!_ARCH}"
        return 1
    fi
    local filename="rust-${version}-${suffix}.tar.gz"

    wget --quiet --show-progress --output-document="${!_SYSTEM_TMP}/${filename}" "https://static.rust-lang.org/dist/${filename}"

    if [ ! -f "${!_SYSTEM_TMP}/${filename}" ]; then
        echo >&2 "Failed to download ${filename} from ${url}"
        return 1
    fi

    tar -xzf "${!_SYSTEM_TMP}/${filename}"

    echo "${filename%.tar.*}::${version}"
}

fetch_perl() {
    local url="https://www.cpan.org/src/5.0/"
    local version="${1}"
    local ext="tar.gz"

    echo $(fetch_web "perl" "${version}" "${url}" "${ext}")
}

fetch_git() {
    # local url="https://www.kernel.org/pub/software/scm/git/"
    local url="https://mirrors.edge.kernel.org/pub/software/scm/git/"
    local version="${1}"
    local ext="${2:-tar.xz}"

    echo $(fetch_web "git" "${version}" "${url}" "${ext}")
}

fetch_github() {
    local repo="$1"
    local version="${2}"

    local pkg="${repo##*/}"
    if ! check_installation "${pkg}"; then
        return 0
    fi

    local tag
    if [[ -n "${version}" ]] && [[ "${version}" != "latest" ]]; then
        tag="v${version}"
    else
        tag=$(curl -s "https://api.github.com/repos/${repo}/releases/latest" 2>/dev/null | jq -r '.tag_name' 2>/dev/null)
        
        if [[ -z "$tag" || "$tag" == "null" ]]; then
            warn "Could not fetch latest version for ${repo}, using fallback"
            return 1
        fi
    fi

    wget --quiet --show-progress --output-document="${!_SYSTEM_TMP}/${pkg}-${tag}.tar.gz" "https://github.com/${repo}/archive/refs/tags/${tag}.tar.gz"

    # Check if the file was downloaded successfully
    if [ ! -f "${!_SYSTEM_TMP}/${pkg}-${tag}.tar.gz" ]; then
        echo >&2 "Failed to download ${pkg} version ${tag} from GitHub"
        return 1
    fi

    tar -xzf "${!_SYSTEM_TMP}/${pkg}-${tag}.tar.gz"

    local version="${tag#v}"
    echo "${pkg}-${tag#v}::${version}"
}

fetch_elfutils() {
    local version="${1}"

    if ! check_installation "elfutils"; then
        return 0
    fi

    if [[ -n "${version}" ]] && [[ "${version}" == "latest" ]]; then
        version=$(curl -s "https://sourceware.org/elfutils/ftp/" \
            | grep -oE "[0-9]+\.[0-9]+(\.[0-9]+)?/" \
            | sort -Vu \
            | tail -n1)
        version="${version%/}"
    fi

    wget --quiet --show-progress --output-document="${!_SYSTEM_TMP}/elfutils-${version}.tar.bz2" "https://sourceware.org/elfutils/ftp/${version}/elfutils-${version}.tar.bz2"

    # Check if the file was downloaded successfully
    if [ ! -f "${!_SYSTEM_TMP}/elfutils-${version}.tar.bz2" ]; then
        echo >&2 "Failed to download elfutils version ${version} from sourceware.org"
        return 1
    fi

    tar -xjf "${!_SYSTEM_TMP}/elfutils-${version}.tar.bz2"

    echo "elfutils-${version}::${version}"
}

fetch_gcc() {
    local version="${1}"
    local ext="${2:-tar.xz}"

    if ! check_installation "gcc"; then
        return 0
    fi

    local basename
    if [[ -n "${version}" ]] && [[ "${version}" != "latest" ]]; then
        basename="gcc-${version}"
    else
        basename=$(curl -sSL "https://ftp.gnu.org/gnu/gcc/" \
            | grep -oE "gcc-[0-9]+\.[0-9]+(\.[0-9]+)?" \
            | sort -Vu \
            | tail -n1)
    fi

    wget --quiet --show-progress --output-document="${!_SYSTEM_TMP}/${basename}.${ext}" "https://ftp.gnu.org/gnu/gcc/${basename}/${basename}.${ext}"

    # Check if the file was downloaded successfully
    if [ ! -f "${!_SYSTEM_TMP}/${basename}.${ext}" ]; then
        echo >&2 "Failed to download ${basename}.${ext} from GNU"
        return 1
    fi

    if [ ${ext} = "tar.xz" ]; then
        tar -xf "${!_SYSTEM_TMP}/${basename}.${ext}"
    elif [ ${ext} = "tar.gz" ]; then
        tar -xzf "${!_SYSTEM_TMP}/${basename}.${ext}"
    elif [ ${ext} = "tar.bz2" ]; then
        tar -xjf "${!_SYSTEM_TMP}/${basename}.${ext}"
    else
        echo >&2 "Unsupported file extension: ${ext}"
        return 1
    fi

    local version="${basename#gcc-}"
    echo "${basename}::${version}"
}
