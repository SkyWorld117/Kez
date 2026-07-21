#!/usr/bin/env bash

# Build the nearly distribution-independent system environment.  Package
# functions are executed by install.sh so independent nodes in the bootstrap
# dependency graph can run concurrently.

source "$(dirname "${BASH_SOURCE[0]}")/colored_io.sh"

build_gmp() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.gmp' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "gmp" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "gmp is already installed, skipping..."
        return 0
    fi

    info "Installing gmp..."
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
    record_package_version "${version}"
}

build_mpfr() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.mpfr' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "mpfr" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "mpfr is already installed, skipping..."
        return 0
    fi

    info "Installing mpfr..."
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
    record_package_version "${version}"
}

build_mpc() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.mpc' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "mpc" "${version}" "tar.gz")
    if [[ -z $folder_and_version ]]; then
        warning "mpc is already installed, skipping..."
        return 0
    fi

    info "Installing mpc..."
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
    record_package_version "${version}"
}

build_isl() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.isl' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_isl "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "isl is already installed, skipping..."
        return 0
    fi

    info "Installing isl..."
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
    record_package_version "${version}"
}

build_zlib() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.zlib' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_github "madler/zlib" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "zlib is already installed, skipping..."
        return 0
    fi

    info "Installing zlib..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure --prefix="${KEZ_SYSTEM}"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    record_package_version "${version}"
}

build_xz() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.xz' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_github "tukaani-project/xz" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "xz is already installed, skipping..."
        return 0
    fi

    info "Installing xz..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./autogen.sh --no-po4a
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    record_package_version "${version}"
}

build_lz4() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.lz4' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_github "lz4/lz4" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "lz4 is already installed, skipping..."
        return 0
    fi

    info "Installing lz4..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    make -j"${KEZ_NPROC}" install PREFIX="${KEZ_SYSTEM}" \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    record_package_version "${version}"
}

build_zstd() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.zstd' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_github "facebook/zstd" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "zstd is already installed, skipping..."
        return 0
    fi

    info "Installing zstd..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    CFLAGS="-I${KEZ_SYSTEM}/include" \
        CXXFLAGS="-I${KEZ_SYSTEM}/include" \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib" \
        make -j"${KEZ_NPROC}" install PREFIX="${KEZ_SYSTEM}" HAVE_ZLIB=1 HAVE_LZ4=1 \
        HAVE_LZMA=1
    record_package_version "${version}"
}

build_binutils() {
    local version folder_and_version folder
    if [[ ${KEZ_INIT_USE_DISTRO_COMPILER} == 1 ]]; then
        warning "Using the system's default compiler, skipping binutils installation..."
        mkdir -p "${KEZ_SYSTEM}/bin"
        ln -sf /usr/bin/ld "${KEZ_SYSTEM}/bin/ld"
        ln -sf /usr/bin/ar "${KEZ_SYSTEM}/bin/ar"
        ln -sf /usr/bin/nm "${KEZ_SYSTEM}/bin/nm"
        ln -sf /usr/bin/objdump "${KEZ_SYSTEM}/bin/objdump"
        ln -sf /usr/bin/strip "${KEZ_SYSTEM}/bin/strip"
        record_package_version distro
        return 0
    fi

    version=$(yq -r '.system-stack.binutils' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "binutils" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "binutils is already installed, skipping..."
        return 0
    fi

    info "Installing binutils..."
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
    record_package_version "${version}"

    hash -r
}

build_gcc() {
    local version folder_and_version folder
    if [[ ${KEZ_INIT_USE_DISTRO_COMPILER} == 1 ]]; then
        warning "Using the system's default compiler, skipping gcc installation..."
        mkdir -p "${KEZ_SYSTEM}/bin"
        ln -sf /usr/bin/gcc "${KEZ_SYSTEM}/bin/gcc"
        ln -sf /usr/bin/g++ "${KEZ_SYSTEM}/bin/g++"
        ln -sf /usr/bin/gfortran "${KEZ_SYSTEM}/bin/gfortran"
        record_package_version distro
        return 0
    fi

    version=$(yq -r '.system-stack.gcc' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gcc "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "gcc is already installed, skipping..."
        return 0
    fi

    info "Installing gcc..."
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
    "${KEZ_HOME}/tools/patch_gcc_linking.sh" "${KEZ_SYSTEM}"
    record_package_version "${version}"

    hash -r
}

build_elfutils() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.elfutils' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_elfutils "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "elfutils is already installed, skipping..."
        return 0
    fi

    info "Installing elfutils..."
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
    record_package_version "${version}"
}

build_m4() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.m4' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "m4" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "m4 is already installed, skipping..."
        return 0
    fi

    info "Installing m4..."
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
    record_package_version "${version}"
}

build_autoconf() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.autoconf' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "autoconf" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "autoconf is already installed, skipping..."
        return 0
    fi

    info "Installing autoconf..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        M4="${KEZ_SYSTEM}/bin/m4" \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    record_package_version "${version}"
}

build_automake() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.automake' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "automake" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "automake is already installed, skipping..."
        return 0
    fi

    info "Installing automake..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    record_package_version "${version}"
}

build_libtool() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.libtool' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "libtool" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "libtool is already installed, skipping..."
        return 0
    fi

    info "Installing libtool..."
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
    record_package_version "${version}"
}

build_make() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.make' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_gnu "make" "${version}" "tar.gz")
    if [[ -z $folder_and_version ]]; then
        warning "make is already installed, skipping..."
        return 0
    fi

    info "Installing make..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --disable-nls \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    record_package_version "${version}"
}

build_cmake() {
    local version cmake_tag cmake_tag_arch
    info "Installing cmake..."
    version=$(yq -r '.system-stack.cmake' "${KEZ_HOME}/manifest.yaml")
    if [[ $version == latest ]]; then
        cmake_tag=$(curl -s "https://api.github.com/repos/Kitware/CMake/releases/latest" |
            jq -r '.tag_name')
    else
        cmake_tag="v${version}"
    fi
    if [[ ${KEZ_ARCH} == x86_64 ]]; then
        cmake_tag_arch="${cmake_tag#v}-linux-x86_64"
    elif [[ ${KEZ_ARCH} == arm64 ]]; then
        cmake_tag_arch="${cmake_tag#v}-linux-aarch64"
    else
        error "Unsupported architecture: ${KEZ_ARCH}"
        return 1
    fi
    wget --quiet --show-progress \
        --output-document="${KEZ_SYSTEM_TMP}/cmake-${cmake_tag_arch}.sh" \
        "https://github.com/Kitware/CMake/releases/download/${cmake_tag}/cmake-${cmake_tag_arch}.sh"
    bash "${KEZ_SYSTEM_TMP}/cmake-${cmake_tag_arch}.sh" --skip-license \
        --prefix="${KEZ_SYSTEM}"
    record_package_version "${cmake_tag#v}"
}

build_rust() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.rust' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_rust "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "rust/cargo is already installed, skipping..."
        return 0
    fi

    info "Installing rust/cargo..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./install.sh --prefix="${KEZ_SYSTEM}" --without=rust-docs
    record_package_version "${version}"
}

build_perl() {
    local version folder_and_version folder bin base_bin unversioned_bin
    version=$(yq -r '.system-stack.perl' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_perl "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "perl is already installed, skipping..."
        return 0
    fi

    info "Installing perl..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./Configure -Dprefix="${KEZ_SYSTEM}" -Dusethreads -Dusedevel -de
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    for bin in "${KEZ_SYSTEM}/bin/"*"${version}"; do
        [[ -e $bin ]] || continue
        base_bin=$(basename "$bin")
        unversioned_bin="${base_bin%%${version}}"
        ln -sf "$bin" "${KEZ_SYSTEM}/bin/${unversioned_bin}"
    done
    record_package_version "${version}"
}

build_git() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.git' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_git "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "git is already installed, skipping..."
        return 0
    fi

    info "Installing git..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        CC="${KEZ_SYSTEM}/bin/gcc" \
        CFLAGS="-O3"
    make all -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    record_package_version "${version}"
}

build_python() {
    local version folder_and_version folder
    version=$(yq -r '.system-stack.python' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_python "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "python is already installed, skipping..."
        return 0
    fi

    info "Installing python..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    ./configure \
        --prefix="${KEZ_SYSTEM}" \
        --enable-shared \
        --enable-optimizations \
        CFLAGS="-O3" \
        LDFLAGS="-L${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"

    # python-is-python3
    ln -sf "${KEZ_SYSTEM}/bin/python3" "${KEZ_SYSTEM}/bin/python"
    ln -sf "${KEZ_SYSTEM}/bin/pip3" "${KEZ_SYSTEM}/bin/pip"
    record_package_version "${version}"

    hash -r
    python -m pip install --upgrade pip
}

build_ninja() {
    local version ninja_tag ninja_tag_arch
    info "Installing ninja..."
    version=$(yq -r '.system-stack.ninja' "${KEZ_HOME}/manifest.yaml")
    if [[ $version == latest ]]; then
        ninja_tag=$(curl -s "https://api.github.com/repos/ninja-build/ninja/releases/latest" 2>/dev/null |
            jq -r '.tag_name' 2>/dev/null)
    else
        ninja_tag="v${version}"
    fi
    if [[ ${KEZ_ARCH} == x86_64 ]]; then
        ninja_tag_arch="ninja-linux.zip"
    elif [[ ${KEZ_ARCH} == arm64 ]]; then
        ninja_tag_arch="ninja-linux-aarch64.zip"
    else
        error "Unsupported architecture: ${KEZ_ARCH}"
        return 1
    fi
    wget --quiet --show-progress --output-document="${KEZ_SYSTEM_TMP}/ninja.zip" \
        "https://github.com/ninja-build/ninja/releases/download/${ninja_tag}/${ninja_tag_arch}"
    unzip -q "${KEZ_SYSTEM_TMP}/ninja.zip" -d "${KEZ_SYSTEM}/bin"
    record_package_version "${ninja_tag#v}"
}

build_meson() {
    local version
    info "Installing meson..."
    version=$(yq -r '.system-stack.meson' "${KEZ_HOME}/manifest.yaml")
    if [[ $version == latest ]]; then
        pip3 install --target="${KEZ_SYSTEM_TMP}" --upgrade meson
    else
        pip3 install --target="${KEZ_SYSTEM_TMP}" --upgrade "meson==${version}"
    fi
    cp -r "${KEZ_SYSTEM_TMP}/"* "${KEZ_SYSTEM}/"
    record_package_version "${version}"
}

build_yaml_cpp() {
    local version folder_and_version folder
    version=$(yq -r '.dependencies.yaml-cpp' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_github "jbeder/yaml-cpp" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "yaml-cpp is already installed, skipping..."
        return 0
    fi

    info "Installing yaml-cpp..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    mkdir build
    cd build
    cmake .. \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -DCMAKE_INSTALL_PREFIX="${KEZ_SYSTEM}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_BUILD_RPATH="${KEZ_SYSTEM}/lib;${KEZ_SYSTEM}/lib64" \
        -DCMAKE_INSTALL_RPATH="${KEZ_SYSTEM}/lib;${KEZ_SYSTEM}/lib64" \
        -DCMAKE_EXE_LINKER_FLAGS="-L${KEZ_SYSTEM}/lib -L${KEZ_SYSTEM}/lib64 -Wl,-rpath,${KEZ_SYSTEM}/lib -Wl,-rpath,${KEZ_SYSTEM}/lib64"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    version="${version#yaml-cpp-}"
    record_package_version "${version}"
}

build_googletest() {
    local version folder_and_version folder
    version=$(yq -r '.dependencies.googletest' "${KEZ_HOME}/manifest.yaml")
    folder_and_version=$(fetch_github "google/googletest" "${version}")
    if [[ -z $folder_and_version ]]; then
        warning "googletest is already installed, skipping..."
        return 0
    fi

    info "Installing googletest..."
    folder="${folder_and_version%%::*}"
    version="${folder_and_version##*::}"
    cd "${KEZ_SYSTEM_TMP}/${folder}"
    mkdir build
    cd build
    cmake .. -DCMAKE_INSTALL_PREFIX="${KEZ_SYSTEM}"
    make -j"${KEZ_NPROC}"
    make install -j"${KEZ_NPROC}"
    record_package_version "${version}"
}

build_patchelf() {
    local version patchelf_tag patchelf_tag_arch
    info "Installing patchelf..."
    version=$(yq -r '.dependencies.patchelf' "${KEZ_HOME}/manifest.yaml")
    if [[ $version == latest ]]; then
        patchelf_tag=$(curl -s \
            "https://api.github.com/repos/NixOS/patchelf/releases/latest" 2>/dev/null |
            jq -r '.tag_name' 2>/dev/null)
    else
        patchelf_tag="v${version}"
    fi
    if [[ ${KEZ_ARCH} == x86_64 ]]; then
        patchelf_tag_arch="${patchelf_tag}-x86_64"
    elif [[ ${KEZ_ARCH} == arm64 ]]; then
        patchelf_tag_arch="${patchelf_tag}-aarch64"
    else
        error "Unsupported architecture: ${KEZ_ARCH}"
        return 1
    fi
    wget --quiet --show-progress --output-document="${KEZ_SYSTEM_TMP}/patchelf.tar.gz" \
        "https://github.com/NixOS/patchelf/releases/download/${patchelf_tag}/patchelf-${patchelf_tag_arch}.tar.gz"
    tar -xzf "${KEZ_SYSTEM_TMP}/patchelf.tar.gz" -C "${KEZ_SYSTEM}"
    record_package_version "${patchelf_tag#v}"
}

init_plan_package() {
    local plan_file=$1
    local package=$2
    local use_distro_compiler=$3
    shift 3
    local dependency command

    printf -v command 'bash %q --build-package %q' "${KEZ_HOME}/scripts/init.sh" "${package}"
    if [[ $use_distro_compiler == 1 ]]; then
        command+=' --use-distro-compiler'
    fi

    printf 'kez_plan_begin %q\n' "${package}" >> "${plan_file}"
    for dependency in "$@"; do
        printf 'kez_plan_depends %q\n' "${dependency}" >> "${plan_file}"
    done
    printf 'kez_plan_command %q\n' "${command}" >> "${plan_file}"
    printf 'kez_plan_end\n' >> "${plan_file}"
}

# Emit the same plan protocol as the C++ backend.  Prebuilt tools have no GCC
# edge and can bootstrap in parallel; every source build after GCC has an
# explicit GCC edge so it cannot accidentally use the distribution compiler.
write_init_plan() {
    local plan_file=$1
    local use_distro_compiler=$2
    printf '# kez-install-plan-v1\n' > "${plan_file}"

    init_plan_package "${plan_file}" gmp "${use_distro_compiler}"
    init_plan_package "${plan_file}" mpfr "${use_distro_compiler}" gmp
    init_plan_package "${plan_file}" mpc "${use_distro_compiler}" gmp mpfr
    init_plan_package "${plan_file}" isl "${use_distro_compiler}" gmp
    init_plan_package "${plan_file}" zlib "${use_distro_compiler}"
    init_plan_package "${plan_file}" xz "${use_distro_compiler}"
    init_plan_package "${plan_file}" lz4 "${use_distro_compiler}"
    init_plan_package "${plan_file}" zstd "${use_distro_compiler}" zlib xz lz4
    init_plan_package "${plan_file}" binutils "${use_distro_compiler}" gmp mpfr mpc isl zstd
    init_plan_package "${plan_file}" gcc "${use_distro_compiler}" binutils

    init_plan_package "${plan_file}" cmake "${use_distro_compiler}"
    init_plan_package "${plan_file}" rust "${use_distro_compiler}"
    init_plan_package "${plan_file}" patchelf "${use_distro_compiler}"
    init_plan_package "${plan_file}" ninja "${use_distro_compiler}"

    init_plan_package "${plan_file}" elfutils "${use_distro_compiler}" gcc
    init_plan_package "${plan_file}" m4 "${use_distro_compiler}" gcc
    init_plan_package "${plan_file}" autoconf "${use_distro_compiler}" gcc m4
    init_plan_package "${plan_file}" automake "${use_distro_compiler}" gcc autoconf
    init_plan_package "${plan_file}" libtool "${use_distro_compiler}" gcc m4
    init_plan_package "${plan_file}" make "${use_distro_compiler}" gcc
    init_plan_package "${plan_file}" perl "${use_distro_compiler}" gcc
    init_plan_package "${plan_file}" git "${use_distro_compiler}" gcc
    init_plan_package "${plan_file}" python "${use_distro_compiler}" gcc
    init_plan_package "${plan_file}" meson "${use_distro_compiler}" python ninja
    init_plan_package "${plan_file}" yaml-cpp "${use_distro_compiler}" gcc cmake
    init_plan_package "${plan_file}" googletest "${use_distro_compiler}" gcc cmake
}

cleanup_init_plan() {
    if [[ -n ${KEZ_INIT_PLAN_FILE:-} ]]; then
        rm -f -- "${KEZ_INIT_PLAN_FILE}"
    fi
}

configure_package_environment() {
    local package=$1

    # All package workers get an executor-owned private scratch directory.
    export KEZ_SYSTEM_TMP=$PWD
    export PATH="${KEZ_SYSTEM}/bin:${PATH}"
    mkdir -p "${KEZ_SYSTEM}" "${KEZ_SYSTEM}/bin" "${KEZ_UTILITIES}" "${KEZ_SYSTEM_TMP}"

    case $package in
        elfutils | m4 | autoconf | automake | libtool | make | perl | git | yaml-cpp | googletest)
            if [[ ! -x ${KEZ_SYSTEM}/bin/gcc || ! -x ${KEZ_SYSTEM}/bin/g++ ]]; then
                error "Package ${package} requires the Kez GCC toolchain"
                return 1
            fi
            export CC="${KEZ_SYSTEM}/bin/gcc"
            export CXX="${KEZ_SYSTEM}/bin/g++"
            export FC="${KEZ_SYSTEM}/bin/gfortran"
            ;;
    esac
}

build_package() {
    local package=$1
    configure_package_environment "${package}"

    case $package in
        gmp) build_gmp ;;
        mpfr) build_mpfr ;;
        mpc) build_mpc ;;
        isl) build_isl ;;
        zlib) build_zlib ;;
        xz) build_xz ;;
        lz4) build_lz4 ;;
        zstd) build_zstd ;;
        binutils) build_binutils ;;
        gcc) build_gcc ;;
        elfutils) build_elfutils ;;
        m4) build_m4 ;;
        autoconf) build_autoconf ;;
        automake) build_automake ;;
        libtool) build_libtool ;;
        make) build_make ;;
        cmake) build_cmake ;;
        rust) build_rust ;;
        perl) build_perl ;;
        git) build_git ;;
        yaml-cpp) build_yaml_cpp ;;
        googletest) build_googletest ;;
        patchelf) build_patchelf ;;
        ninja) build_ninja ;;
        python) build_python ;;
        meson) build_meson ;;
        *)
            error "Unknown system package: ${package}"
            return 2
            ;;
    esac
}

set_init_paths() {
    : "${KEZ_HOME:?KEZ_HOME is not set}"
    : "${KEZ_WORKDIR:?KEZ_WORKDIR is not set}"
    export KEZ_SYSTEM="${KEZ_WORKDIR}/$(yq -r '.paths.system' "${KEZ_HOME}/manifest.yaml")"
    export KEZ_UTILITIES="${KEZ_WORKDIR}/$(yq -r '.paths.utilities' "${KEZ_HOME}/manifest.yaml")"
}

run_package_mode() {
    local package=$1
    shift
    KEZ_INIT_USE_DISTRO_COMPILER=0
    if [[ ${1:-} == --use-distro-compiler ]]; then
        KEZ_INIT_USE_DISTRO_COMPILER=1
        shift
    fi
    if (( $# != 0 )); then
        error "Unknown package build option: $1"
        return 2
    fi
    export KEZ_INIT_USE_DISTRO_COMPILER

    unset CFLAGS CXXFLAGS LDFLAGS CPPFLAGS
    unset LIBRARY_PATH LD_LIBRARY_PATH PKG_CONFIG_PATH
    unset CMAKE_PREFIX_PATH CMAKE_LIBRARY_PATH CMAKE_INCLUDE_PATH
    set_init_paths
    source "${KEZ_HOME}/scripts/init_utils.sh"
    build_package "${package}"
}

main() {
    local force=0
    local use_distro_compiler=0
    local argument plan_file

    for argument in "$@"; do
        case $argument in
            --force) force=1 ;;
            --use-distro-compiler) use_distro_compiler=1 ;;
            *)
                error "Unknown init option: ${argument}"
                return 2
                ;;
        esac
    done

    : "${KEZ_NPROC:?KEZ_NPROC is not set}"
    if [[ ! $KEZ_NPROC =~ ^[1-9][0-9]*$ ]]; then
        error "KEZ_NPROC must be a positive integer"
        return 2
    fi

    unset CFLAGS CXXFLAGS LDFLAGS CPPFLAGS
    unset LIBRARY_PATH LD_LIBRARY_PATH PKG_CONFIG_PATH
    unset CMAKE_PREFIX_PATH CMAKE_LIBRARY_PATH CMAKE_INCLUDE_PATH
    set_init_paths
    local system_tmp="${KEZ_SYSTEM}/.tmp"
    mkdir -p "${KEZ_SYSTEM}" "${KEZ_UTILITIES}" "${system_tmp}"

    if (( force == 1 )); then
        rm -rf "${KEZ_SYSTEM:?}/"*
        rm -rf "${system_tmp:?}/"*
        mkdir -p "${KEZ_SYSTEM}" "${KEZ_SYSTEM}/bin" "${system_tmp}"
    fi
    if [[ ! -f ${KEZ_SYSTEM}/state.yaml ]]; then
        printf 'state:\n' > "${KEZ_SYSTEM}/state.yaml"
    fi

    plan_file=$(mktemp "${system_tmp}/init-plan.XXXXXX")
    KEZ_INIT_PLAN_FILE=${plan_file}
    trap cleanup_init_plan EXIT
    write_init_plan "${plan_file}" "${use_distro_compiler}"

    KEZ_INSTALL_STATE_FORMAT=map \
        bash "${KEZ_HOME}/scripts/install.sh" "${KEZ_SYSTEM}" "${plan_file}"

    export PATH="${KEZ_SYSTEM}/bin:${PATH}"
    make -C "${KEZ_HOME}" -j"${KEZ_NPROC}"
    "${KEZ_HOME}/bin/kez_print" success "Kez environment initialization complete."

    hash -r
}

if [[ ${BASH_SOURCE[0]} == "$0" ]]; then
    set -Eeuo pipefail
    if [[ ${1:-} == --build-package ]]; then
        if (( $# < 2 )); then
            error "Usage: init.sh --build-package <package> [--use-distro-compiler]"
            exit 2
        fi
        package=$2
        shift 2
        run_package_mode "${package}" "$@"
    else
        main "$@"
    fi
fi
