#!/usr/bin/env bash

# Remove some problematic environment variables that may interfere with the installation process
unset CFLAGS CXXFLAGS LDFLAGS CPPFLAGS
unset LIBRARY_PATH LD_LIBRARY_PATH
unset PKG_CONFIG_PATH
unset CMAKE_PREFIX_PATH CMAKE_LIBRARY_PATH CMAKE_INCLUDE_PATH

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Define the work directory and other environment variables
PROJECT=$(yq -r '.project.name' "${SCRIPT_DIR}/../manifest.yaml")
PREFIX=${PROJECT^^}  # Convert to uppercase

_WORKDIR="${PREFIX}_WORKDIR"
_HOME="${PREFIX}_HOME"
_DB="${PREFIX}_DB"
_ENV="${PREFIX}_ENV"
_ARCH="${PREFIX}_ARCH"
_NPROC="${PREFIX}_NPROC"

_SYSTEM="${PREFIX}_SYSTEM"
_SYSTEM_TMP="${PREFIX}_SYSTEM_TMP"
_UTILITIES="${PREFIX}_UTILITIES"

# This script creates a nearly distribution-independent environment.
# This environment offers the essential system utilities.

eval "${_SYSTEM}"="${!_WORKDIR}/$(yq -r '.paths.system' "${!_HOME}/manifest.yaml")"
eval "${_UTILITIES}"="${!_WORKDIR}/$(yq -r '.paths.utilities' "${!_HOME}/manifest.yaml")"
eval "${_SYSTEM_TMP}"="${!_SYSTEM}/.tmp"

mkdir -p "${!_SYSTEM}" "${!_UTILITIES}" "${!_SYSTEM_TMP}"

source ${!_HOME}/scripts/init_utils.sh

if [ ! -f "${!_SYSTEM}/state.yaml" ]; then
    echo "state:" > "${!_SYSTEM}/state.yaml"
fi

if [ "${1:-}" == "--refresh" ] || [ "${2:-}" == "--refresh" ]; then
    if [ -d "${!_SYSTEM}" ]; then
        rm -rf "${!_SYSTEM}"/*
        rm -rf "${!_SYSTEM_TMP}"/*
    fi
    mkdir -p "${!_SYSTEM}"
    echo "state:" > "${!_SYSTEM}/state.yaml"
fi

use_distro_compiler=0
if [ "${1:-}" == "--use-distro-compiler" ] || [ "${2:-}" == "--use-distro-compiler" ]; then
    use_distro_compiler=1
fi

cd "${!_SYSTEM_TMP}"


# PHASE 1: Mathematical Libraries
# ----------------------------------------------------

# gmp
version=$(yq -r '.system-stack.gmp' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "gmp" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing gmp..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --enable-cxx \
        --enable-shared \
        --enable-static
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  gmp: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "gmp is already installed, skipping..."
fi

# mpfr
version=$(yq -r '.system-stack.mpfr' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "mpfr" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing mpfr..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --with-gmp="${!_SYSTEM}" \
        --enable-shared \
        --enable-static \
        --enable-float128 \
        LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  mpfr: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "mpfr is already installed, skipping..."
fi

# mpc
version=$(yq -r '.system-stack.mpc' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "mpc" "${version}" "tar.gz")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing mpc..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --with-gmp="${!_SYSTEM}" \
        --with-mpfr="${!_SYSTEM}" \
        --enable-shared \
        --enable-static \
        LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  mpc: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "mpc is already installed, skipping..."
fi

# isl
version=$(yq -r '.system-stack.isl' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_isl "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing isl..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --enable-shared \
        --enable-static \
        --with-gmp-prefix="${!_SYSTEM}" \
        --with-int=gmp \
        LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  isl: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "isl is already installed, skipping..."
fi


# PHASE 2: Compression Libraries
# ----------------------------------------------------

# zlib
version=$(yq -r '.system-stack.zlib' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "madler/zlib" "${version}")
if check_installation "zlib"; then
    echo "Installing zlib..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  zlib: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "zlib is already installed, skipping..."
fi

# xz
version=$(yq -r '.system-stack.xz' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "tukaani-project/xz" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing xz..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./autogen.sh --no-po4a
    ./configure \
        --prefix="${!_SYSTEM}" \
        LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  xz: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "xz is already installed, skipping..."
fi

# lz4
version=$(yq -r '.system-stack.lz4' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "lz4/lz4" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing lz4..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    make -j"${!_NPROC}" install PREFIX="${!_SYSTEM}" LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  lz4: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "lz4 is already installed, skipping..."
fi

# zstd
version=$(yq -r '.system-stack.zstd' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "facebook/zstd" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing zstd..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    CFLAGS="-I${!_SYSTEM}/include" \
    CXXFLAGS="-I${!_SYSTEM}/include" \
    LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib" \
    make -j"${!_NPROC}" install PREFIX="${!_SYSTEM}" HAVE_ZLIB=1 HAVE_LZ4=1 HAVE_LZMA=1
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  zstd: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "zstd is already installed, skipping..."
fi


# PHASE 3: Essential System Tools
# ----------------------------------------------------

# binutils (without headers)
version=$(yq -r '.system-stack.binutils' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "binutils" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing binutils..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --includedir="${!_SYSTEM_TMP}/include" \
        --disable-multilib \
        --disable-nls \
        --enable-gold \
        --enable-ld \
        --enable-compressed-debug-sections=all \
        --enable-default-compressed-debug-sections-algorithm=zstd \
        --with-gmp="${!_SYSTEM}" \
        --with-mpfr="${!_SYSTEM}" \
        --with-mpc="${!_SYSTEM}" \
        --with-isl="${!_SYSTEM}" \
        --with-zstd="${!_SYSTEM}" \
        LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  binutils: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "binutils is already installed, skipping..."
fi

# gcc
if [[ use_distro_compiler -eq 1 ]]; then
    echo "Using the system's default compiler, skipping gcc installation..."
    if check_installation "gcc"; then
        ln -sf /usr/bin/gcc "${!_SYSTEM}/bin/gcc"
        ln -sf /usr/bin/g++ "${!_SYSTEM}/bin/g++"
        ln -sf /usr/bin/gfortran "${!_SYSTEM}/bin/gfortran"
        echo -e "  gcc: distro" >> "${!_SYSTEM}/state.yaml"
    fi
else
    version=$(yq -r '.system-stack.gcc' "${!_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gcc "${version}")
    if [[ -n "$folder_and_version" ]]; then
        echo "Installing gcc..."
        folder="${folder_and_version%%::*}"
        version="${folder_and_version##*::}"
        cd "${!_SYSTEM_TMP}/${folder}"
        ./configure \
            --prefix="${!_SYSTEM}" \
            --enable-languages=c,c++,fortran \
            --disable-multilib \
            --disable-nls \
            --with-gmp="${!_SYSTEM}" \
            --with-mpfr="${!_SYSTEM}" \
            --with-mpc="${!_SYSTEM}" \
            --with-isl="${!_SYSTEM}" \
            --with-zstd-include="${!_SYSTEM}/include" \
            --with-zstd-lib="${!_SYSTEM}/lib" \
            --with-stage1-ldflags="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib -L${!_SYSTEM}/lib64 -Wl,-rpath,${!_SYSTEM}/lib64" \
            --with-boot-ldflags="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib -L${!_SYSTEM}/lib64 -Wl,-rpath,${!_SYSTEM}/lib64"
        export LD_RUN_PATH="${!_SYSTEM}/lib:${!_SYSTEM}/lib64"
        make -j"${!_NPROC}"
        make install -j"${!_NPROC}"
        unset LD_RUN_PATH

        ${!_HOME}/tools/patch_gcc_linking.sh "${!_SYSTEM}"

        cd "${!_SYSTEM_TMP}" && rm -rf *
        echo -e "  gcc: ${version}" >> "${!_SYSTEM}/state.yaml"
    else
        echo "gcc is already installed, skipping..."
    fi
fi

# elfutils
version=$(yq -r '.system-stack.elfutils' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_elfutils "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing elfutils..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --enable-install-elfh \
        --disable-nls \
        LDFLAGS="-Wl,-rpath,${!_SYSTEM}/lib -L${!_SYSTEM}/lib64 -Wl,-rpath,${!_SYSTEM}/lib64"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  elfutils: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "elfutils is already installed, skipping..."
fi


# PHASE 4: Build Tools
# ----------------------------------------------------

# m4
version=$(yq -r '.system-stack.m4' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "m4" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing m4..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --enable-c++ \
        --enable-threads=isoc+posix \
        --disable-nls \
    LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  m4: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "m4 is already installed, skipping..."
fi

# autoconf
version=$(yq -r '.system-stack.autoconf' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "autoconf" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing autoconf..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
    M4="${!_SYSTEM}/bin/m4" \
    LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  autoconf: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "autoconf is already installed, skipping..."
fi

# automake
version=$(yq -r '.system-stack.automake' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "automake" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing automake..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  automake: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "automake is already installed, skipping..."
fi

# libtool
version=$(yq -r '.system-stack.libtool' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "libtool" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing libtool..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --enable-shared \
        --enable-static \
        LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  libtool: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "libtool is already installed, skipping..."
fi

# make
version=$(yq -r '.system-stack.make' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "make" "${version}" "tar.gz")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing make..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        --disable-nls \
        LDFLAGS="-L${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  make: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "make is already installed, skipping..."
fi

# cmake
if check_installation "cmake"; then
    echo "Installing cmake..."
    version=$(yq -r '.system-stack.cmake' "${!_HOME}/manifest.yaml")
    if [[ "${version}" == "latest" ]]; then
        cmake_tag="$(curl -s "https://api.github.com/repos/Kitware/CMake/releases/latest" | jq -r '.tag_name')"
    else
        cmake_tag="v${version}"
    fi
    if [ "${!_ARCH}" = "x86_64" ]; then
        cmake_tag_arch="${cmake_tag#v}-linux-x86_64"
    elif [ "${!_ARCH}" = "arm64" ]; then
        cmake_tag_arch="${cmake_tag#v}-linux-aarch64"
    fi
    wget --quiet --show-progress --output-document="${!_SYSTEM_TMP}/cmake-${cmake_tag_arch}.sh" "https://github.com/Kitware/CMake/releases/download/${cmake_tag}/cmake-${cmake_tag_arch}.sh"
    bash "${!_SYSTEM_TMP}/cmake-${cmake_tag_arch}.sh" --skip-license --prefix="${!_SYSTEM}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  cmake: ${cmake_tag#v}" >> "${!_SYSTEM}/state.yaml"
else
    echo "cmake is already installed, skipping..."
fi

# rust/cargo
version=$(yq -r '.system-stack.rust' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_rust "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing rust/cargo..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./install.sh --prefix="${!_SYSTEM}" --without=rust-docs
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  rust: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "rust/cargo is already installed, skipping..."
fi

# perl
version=$(yq -r '.system-stack.perl' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_perl "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing perl..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./Configure -Dprefix="${!_SYSTEM}" -Dusethreads -Dusedevel -de
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    for bin in "${!_SYSTEM}/bin/"*${version}; do
        base_bin="$(basename "$bin")"
        unversioned_bin="${base_bin%%${version}}"
        ln -sf "$bin" "${!_SYSTEM}/bin/${unversioned_bin}"
    done
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  perl: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "perl is already installed, skipping..."
fi

# git
version=$(yq -r '.system-stack.git' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_git "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing git..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${!_SYSTEM}" \
        CC="${!_SYSTEM}/bin/gcc" \
        CFLAGS="-O3"
    make all -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  git: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "git is already installed, skipping..."
fi


# PHASE 5: Essential Tools for Fromager
# ----------------------------------------------------

# yaml-cpp
version=$(yq -r '.dependencies.yaml-cpp' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "jbeder/yaml-cpp" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing yaml-cpp..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    mkdir build && cd build
    cmake .. \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_INSTALL_PREFIX="${!_SYSTEM}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DYAML_BUILD_SHARED_LIBS=ON \
        -DCMAKE_BUILD_RPATH="${!_SYSTEM}/lib;${!_SYSTEM}/lib64" \
        -DCMAKE_INSTALL_RPATH="${!_SYSTEM}/lib;${!_SYSTEM}/lib64" \
        -DCMAKE_EXE_LINKER_FLAGS="-L${!_SYSTEM}/lib -L${!_SYSTEM}/lib64 -Wl,-rpath,${!_SYSTEM}/lib -Wl,-rpath,${!_SYSTEM}/lib64"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    version=${version#yaml-cpp-}
    echo -e "  yaml-cpp: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "yaml-cpp is already installed, skipping..."
fi

# argparse
version=$(yq -r '.dependencies.argparse' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "p-ranav/argparse" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing argparse..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    cp -r include/argparse "${!_SYSTEM}/include/argparse"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  argparse: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "argparse is already installed, skipping..."
fi

# googletest
version=$(yq -r '.dependencies.googletest' "${!_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "google/googletest" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing googletest..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${!_SYSTEM_TMP}/${folder}"
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX="${!_SYSTEM}"
    make -j"${!_NPROC}"
    make install -j"${!_NPROC}"
    cd "${!_SYSTEM_TMP}" && rm -rf *
    echo -e "  googletest: ${version}" >> "${!_SYSTEM}/state.yaml"
else
    echo "googletest is already installed, skipping..."
fi

# patchelf
if check_installation "patchelf"; then
    echo "Installing patchelf..."
    version=$(yq -r '.dependencies.patchelf' "${!_HOME}/manifest.yaml")
    if [[ "${version}" == "latest" ]]; then
        patchelf_tag="$(curl -s "https://api.github.com/repos/NixOS/patchelf/releases/latest" 2>/dev/null | jq -r '.tag_name' 2>/dev/null)"
    else
        patchelf_tag="v${version}"
    fi
    patchelf_tag_arch=""
    if [ "${!_ARCH}" = "x86_64" ]; then
        patchelf_tag_arch="${patchelf_tag}-x86_64"
    elif [ "${!_ARCH}" = "arm64" ]; then
        patchelf_tag_arch="${patchelf_tag}-aarch64"
    fi
    wget --quiet --show-progress --output-document="${!_SYSTEM_TMP}/patchelf.tar.gz" "https://github.com/NixOS/patchelf/releases/download/${patchelf_tag}/patchelf-${patchelf_tag_arch}.tar.gz"
    tar -xzf "${!_SYSTEM_TMP}/patchelf.tar.gz" -C "${!_SYSTEM}"
    echo -e "  patchelf: ${patchelf_tag#v}" >> "${!_SYSTEM}/state.yaml"
else
    echo "patchelf is already installed, skipping..."
fi

# Fromager
# make -C "${!_SYSTEM}" -B -j"${!_NPROC}"


# End of Script
# ----------------------------------------------------
echo "${PROJECT} environment initialization complete."

set +Eeuo pipefail
