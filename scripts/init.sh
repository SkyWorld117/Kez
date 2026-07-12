#!/usr/bin/env bash

# Remove some problematic environment variables that may interfere with the installation process
unset CFLAGS CXXFLAGS LDFLAGS CPPFLAGS
unset LIBRARY_PATH LD_LIBRARY_PATH
unset PKG_CONFIG_PATH
unset CMAKE_PREFIX_PATH CMAKE_LIBRARY_PATH CMAKE_INCLUDE_PATH

set -Eeuo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# This script creates a nearly distribution-independent environment.
# This environment offers the essential system utilities.

KEZ_SYSTEM="${KEZ_WORKDIR}/$(yq -r '.paths.system' "${KEZ_HOME}/manifest.yaml")"
KEZ_UTILITIES="${KEZ_WORKDIR}/$(yq -r '.paths.utilities' "${KEZ_HOME}/manifest.yaml")"
KEZ_SYSTEM_TMP="${KEZ_SYSTEM}/.tmp"

mkdir -p "${KEZ_SYSTEM}" "${KEZ_UTILITIES}" "${KEZ_SYSTEM_TMP}"

source ${KEZ_HOME}/scripts/init_utils.sh

if [ ! -f "${KEZ_SYSTEM}/state.yaml" ]; then
    echo "state:" > "${KEZ_SYSTEM}/state.yaml"
fi

if [ "${1:-}" == "--refresh" ] || [ "${2:-}" == "--refresh" ]; then
    if [ -d "${KEZ_SYSTEM}" ]; then
        rm -rf "${KEZ_SYSTEM}"/*
        rm -rf "${KEZ_SYSTEM_TMP}"/*
    fi
    mkdir -p "${KEZ_SYSTEM}"
    echo "state:" > "${KEZ_SYSTEM}/state.yaml"
fi

use_distro_compiler=0
if [ "${1:-}" == "--use-distro-compiler" ] || [ "${2:-}" == "--use-distro-compiler" ]; then
    use_distro_compiler=1
fi

cd "${KEZ_SYSTEM_TMP}"


# PHASE 1: Mathematical Libraries
# ----------------------------------------------------

# gmp
version=$(yq -r '.system-stack.gmp' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "gmp" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing gmp..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --enable-cxx \
        --enable-shared \
        --enable-static
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  gmp: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "gmp is already installed, skipping..."
fi

# mpfr
version=$(yq -r '.system-stack.mpfr' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "mpfr" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing mpfr..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --with-gmp="${KEZ_SYSTEM}" \
        --enable-shared \
        --enable-static \
        --enable-float128 \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  mpfr: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "mpfr is already installed, skipping..."
fi

# mpc
version=$(yq -r '.system-stack.mpc' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "mpc" "${version}" "tar.gz")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing mpc..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --with-gmp="${KEZ_SYSTEM}" \
        --with-mpfr="${KEZ_SYSTEM}" \
        --enable-shared \
        --enable-static \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  mpc: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "mpc is already installed, skipping..."
fi

# isl
version=$(yq -r '.system-stack.isl' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_isl "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing isl..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --enable-shared \
        --enable-static \
        --with-gmp-prefix="${KEZ_SYSTEM}" \
        --with-int=gmp \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  isl: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "isl is already installed, skipping..."
fi


# PHASE 2: Compression Libraries
# ----------------------------------------------------

# zlib
version=$(yq -r '.system-stack.zlib' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "madler/zlib" "${version}")
if check_installation "zlib"; then
    echo "Installing zlib..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  zlib: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "zlib is already installed, skipping..."
fi

# xz
version=$(yq -r '.system-stack.xz' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "tukaani-project/xz" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing xz..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./autogen.sh --no-po4a
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  xz: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "xz is already installed, skipping..."
fi

# lz4
version=$(yq -r '.system-stack.lz4' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "lz4/lz4" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing lz4..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    make -j"${KEZ_NPROC}" install PREFIX="${KEZ_SYSTEM}" LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  lz4: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "lz4 is already installed, skipping..."
fi

# zstd
version=$(yq -r '.system-stack.zstd' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "facebook/zstd" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing zstd..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    CFLAGS="-I${KEZ_SYSTEM}/include" \
    CXXFLAGS="-I${KEZ_SYSTEM}/include" \
    LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib" \
    make -j"${KEZ_NPROC}" install PREFIX="${KEZ_SYSTEM}" HAVE_ZLIB=1 HAVE_LZ4=1 HAVE_LZMA=1
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  zstd: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "zstd is already installed, skipping..."
fi


# PHASE 3: Essential System Tools
# ----------------------------------------------------

# binutils (without headers)
if [[ use_distro_compiler -eq 1 ]]; then
    echo "Using the system's default compiler, skipping binutils installation..."
    if check_installation "ld"; then
        ln -sf /usr/bin/ld "${KEZ_SYSTEM}/bin/ld"
        ln -sf /usr/bin/ar "${KEZ_SYSTEM}/bin/ar"
        ln -sf /usr/bin/nm "${KEZ_SYSTEM}/bin/nm"
        ln -sf /usr/bin/objdump "${KEZ_SYSTEM}/bin/objdump"
        ln -sf /usr/bin/strip "${KEZ_SYSTEM}/bin/strip"
        echo -e "  binutils: distro" >> "${KEZ_SYSTEM}/state.yaml"
    fi
else
    version=$(yq -r '.system-stack.binutils' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "binutils" "${version}")
    if [[ -n "$folder_and_version" ]]; then
        echo "Installing binutils..."
        folder="${folder_and_version%%::*}"
        version="${folder_and_version##*::}"
        cd "${KEZ_SYSTEM_TMP}/${folder}"
        ./configure \
            --prefix="${KEZ_SYSTEM}" \
            --includedir="${KEZ_SYSTEM_TMP}/include" \
            --disable-multilib \
            --disable-nls \
            --enable-gold \
            --enable-ld \
            --enable-compressed-debug-sections=all \
            --enable-default-compressed-debug-sections-algorithm=zstd \
            --with-gmp="${KEZ_SYSTEM}" \
            --with-mpfr="${KEZ_SYSTEM}" \
            --with-mpc="${KEZ_SYSTEM}" \
            --with-isl="${KEZ_SYSTEM}" \
            --with-zstd="${KEZ_SYSTEM}" \
            LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
        make -j"${KEZ_NPROC}"
        make install -j"${KEZ_NPROC}"
        cd "${KEZ_SYSTEM_TMP}" && rm -rf *
        echo -e "  binutils: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
    else
        echo "binutils is already installed, skipping..."
    fi
fi

# gcc
if [[ use_distro_compiler -eq 1 ]]; then
    echo "Using the system's default compiler, skipping gcc installation..."
    if check_installation "gcc"; then
        ln -sf /usr/bin/gcc "${KEZ_SYSTEM}/bin/gcc"
        ln -sf /usr/bin/g++ "${KEZ_SYSTEM}/bin/g++"
        ln -sf /usr/bin/gfortran "${KEZ_SYSTEM}/bin/gfortran"
        echo -e "  gcc: distro" >> "${KEZ_SYSTEM}/state.yaml"
    fi
else
    version=$(yq -r '.system-stack.gcc' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gcc "${version}")
    if [[ -n "$folder_and_version" ]]; then
        echo "Installing gcc..."
        folder="${folder_and_version%%::*}"
        version="${folder_and_version##*::}"
        cd "${KEZ_SYSTEM_TMP}/${folder}"
        ./configure \
            --prefix="${KEZ_SYSTEM}" \
            --enable-languages=c,c++,fortran \
            --disable-multilib \
            --disable-nls \
            --with-gmp="${KEZ_SYSTEM}" \
            --with-mpfr="${KEZ_SYSTEM}" \
            --with-mpc="${KEZ_SYSTEM}" \
            --with-isl="${KEZ_SYSTEM}" \
            --with-zstd-include="${KEZ_SYSTEM}/include" \
            --with-zstd-lib="${KEZ_SYSTEM}/lib" \
            --with-stage1-ldflags="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib -L${KEZ_SYSTEM}/lib64 -Wl,-rpath,${KEZ_SYSTEM}/lib64" \
            --with-boot-ldflags="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib -L${KEZ_SYSTEM}/lib64 -Wl,-rpath,${KEZ_SYSTEM}/lib64"
        export LD_RUN_PATH="${KEZ_SYSTEM}/lib:${KEZ_SYSTEM}/lib64"
        make -j"${KEZ_NPROC}"
        make install -j"${KEZ_NPROC}"
        unset LD_RUN_PATH

        ${KEZ_HOME}/tools/patch_gcc_linking.sh "${KEZ_SYSTEM}"

        cd "${KEZ_SYSTEM_TMP}" && rm -rf *
        echo -e "  gcc: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
    else
        echo "gcc is already installed, skipping..."
    fi
fi

# elfutils
version=$(yq -r '.system-stack.elfutils' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_elfutils "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing elfutils..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --enable-install-elfh \
        --disable-nls \
        LDFLAGS="-Wl,-rpath,${KEZ_SYSTEM}/lib -L${KEZ_SYSTEM}/lib64 -Wl,-rpath,${KEZ_SYSTEM}/lib64"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  elfutils: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "elfutils is already installed, skipping..."
fi


# PHASE 4: Build Tools
# ----------------------------------------------------

# m4
version=$(yq -r '.system-stack.m4' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "m4" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing m4..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --enable-c++ \
        --enable-threads=isoc+posix \
        --disable-nls \
    LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  m4: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "m4 is already installed, skipping..."
fi

# autoconf
version=$(yq -r '.system-stack.autoconf' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "autoconf" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing autoconf..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
    M4="${KEZ_SYSTEM}/bin/m4" \
    LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  autoconf: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "autoconf is already installed, skipping..."
fi

# automake
version=$(yq -r '.system-stack.automake' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "automake" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing automake..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  automake: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "automake is already installed, skipping..."
fi

# libtool
version=$(yq -r '.system-stack.libtool' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "libtool" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing libtool..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --enable-shared \
        --enable-static \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  libtool: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "libtool is already installed, skipping..."
fi

# make
version=$(yq -r '.system-stack.make' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_gnu "make" "${version}" "tar.gz")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing make..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --disable-nls \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  make: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "make is already installed, skipping..."
fi

# cmake
if check_installation "cmake"; then
    echo "Installing cmake..."
    version=$(yq -r '.system-stack.cmake' "${KEZ_HOME}/manifest.yaml")
    if [[ "${version}" == "latest" ]]; then
        cmake_tag="$(curl -s "https://api.github.com/repos/Kitware/CMake/releases/latest" | jq -r '.tag_name')"
    else
        cmake_tag="v${version}"
    fi
    if [ "${KEZ_ARCH}" = "x86_64" ]; then
        cmake_tag_arch="${cmake_tag#v}-linux-x86_64"
    elif [ "${KEZ_ARCH}" = "arm64" ]; then
        cmake_tag_arch="${cmake_tag#v}-linux-aarch64"
    fi
    wget --quiet --show-progress --output-document="${KEZ_SYSTEM_TMP}/cmake-${cmake_tag_arch}.sh" "https://github.com/Kitware/CMake/releases/download/${cmake_tag}/cmake-${cmake_tag_arch}.sh"
    bash "${KEZ_SYSTEM_TMP}/cmake-${cmake_tag_arch}.sh" --skip-license --prefix="${KEZ_SYSTEM}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  cmake: ${cmake_tag#v}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "cmake is already installed, skipping..."
fi

# rust/cargo
version=$(yq -r '.system-stack.rust' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_rust "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing rust/cargo..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./install.sh --prefix="${KEZ_SYSTEM}" --without=rust-docs
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  rust: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "rust/cargo is already installed, skipping..."
fi

# perl
version=$(yq -r '.system-stack.perl' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_perl "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing perl..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./Configure -Dprefix="${KEZ_SYSTEM}" -Dusethreads -Dusedevel -de
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    for bin in "${KEZ_SYSTEM}/bin/"*${version}; do
        base_bin="$(basename "$bin")"
        unversioned_bin="${base_bin%%${version}}"
        ln -sf "$bin" "${KEZ_SYSTEM}/bin/${unversioned_bin}"
    done
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  perl: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "perl is already installed, skipping..."
fi

# git
version=$(yq -r '.system-stack.git' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_git "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing git..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        CC="${KEZ_SYSTEM}/bin/gcc" \
        CFLAGS="-O3"
    make all -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  git: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "git is already installed, skipping..."
fi


# PHASE 5: Essential Tools for Kez
# ----------------------------------------------------

# yaml-cpp
version=$(yq -r '.dependencies.yaml-cpp' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "jbeder/yaml-cpp" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing yaml-cpp..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    mkdir build && cd build
    cmake .. \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_INSTALL_PREFIX="${KEZ_SYSTEM}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_BUILD_RPATH="${KEZ_SYSTEM}/lib;${KEZ_SYSTEM}/lib64" \
        -DCMAKE_INSTALL_RPATH="${KEZ_SYSTEM}/lib;${KEZ_SYSTEM}/lib64" \
        -DCMAKE_EXE_LINKER_FLAGS="-L${KEZ_SYSTEM}/lib -L${KEZ_SYSTEM}/lib64 -Wl,-rpath,${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib64"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    version=${version#yaml-cpp-}
    echo -e "  yaml-cpp: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "yaml-cpp is already installed, skipping..."
fi

# googletest
version=$(yq -r '.dependencies.googletest' "${KEZ_HOME}/manifest.yaml")
folder_and_version=$(fetch_github "google/googletest" "${version}")
if [[ -n "$folder_and_version" ]]; then
    echo "Installing googletest..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX="${KEZ_SYSTEM}"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    cd "${KEZ_SYSTEM_TMP}" && rm -rf *
    echo -e "  googletest: ${version}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "googletest is already installed, skipping..."
fi

# patchelf
if check_installation "patchelf"; then
    echo "Installing patchelf..."
    version=$(yq -r '.dependencies.patchelf' "${KEZ_HOME}/manifest.yaml")
    if [[ "${version}" == "latest" ]]; then
        patchelf_tag="$(curl -s "https://api.github.com/repos/NixOS/patchelf/releases/latest" 2>/dev/null | jq -r '.tag_name' 2>/dev/null)"
    else
        patchelf_tag="v${version}"
    fi
    patchelf_tag_arch=""
    if [ "${KEZ_ARCH}" = "x86_64" ]; then
        patchelf_tag_arch="${patchelf_tag}-x86_64"
    elif [ "${KEZ_ARCH}" = "arm64" ]; then
        patchelf_tag_arch="${patchelf_tag}-aarch64"
    fi
    wget --quiet --show-progress --output-document="${KEZ_SYSTEM_TMP}/patchelf.tar.gz" "https://github.com/NixOS/patchelf/releases/download/${patchelf_tag}/patchelf-${patchelf_tag_arch}.tar.gz"
    tar -xzf "${KEZ_SYSTEM_TMP}/patchelf.tar.gz" -C "${KEZ_SYSTEM}"
    echo -e "  patchelf: ${patchelf_tag#v}" >> "${KEZ_SYSTEM}/state.yaml"
else
    echo "patchelf is already installed, skipping..."
fi

# Kez
make -C "${KEZ_HOME}" -B -j"${KEZ_NPROC}"


# End of Script
# ----------------------------------------------------
${KEZ_HOME}/bin/kez_print success "Kez environment initialization complete."

set +Eeuo pipefail
