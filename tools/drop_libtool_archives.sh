#!/usr/bin/env bash

# Drop every libtool archive (*.la) from an installation prefix.
#
# libtool resolves -lfoo to foo.la in preference to foo.so and then splices
# that archive's dependency_libs onto the link line as explicit arguments.
# The linker defaults to --no-as-needed, so each spliced argument earns a
# DT_NEEDED entry even when the library references none of its symbols, and
# libtool emits -Wl,-rpath for every spliced archive's libdir.  The archives
# also record stale libdir= paths and, for vendor archives, absolute paths
# from the machine that built them.  Removing the archives removes all of it;
# consumers still receive the same libraries transitively through the DT_NEEDED
# entries of the libraries that genuinely reference them.
#
# This mirrors Spack, whose autotools builder deletes the archives by default
# (AutotoolsBuilder.install_libtool_archives = False).  Set
# KEZ_KEEP_LIBTOOL_ARCHIVES=1 for a package that must support libtool static
# linking, where the archives carry the transitive library list.

set -Eeuo pipefail

KEZ_PRINT=""
if [ -n "${KEZ_HOME:-}" ] && [ -x "${KEZ_HOME}/bin/kez_print" ]; then
    KEZ_PRINT="${KEZ_HOME}/bin/kez_print"
fi

log() {
    if [ -n "${KEZ_PRINT}" ]; then
        "${KEZ_PRINT}" "$1" "$2"
    else
        echo "$2"
    fi
}

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <prefix>" >&2
    exit 1
fi

PREFIX="$1"

# A prefix with no directory holds no archives; that is not an error, since
# this runs for every package regardless of whether it installs a library.
if [ ! -d "${PREFIX}" ]; then
    log warning "No such prefix, nothing to clean: ${PREFIX}"
    exit 0
fi

if [ -n "${KEZ_KEEP_LIBTOOL_ARCHIVES:-}" ]; then
    exit 0
fi

ARCHIVES=()
while IFS= read -r -d '' archive; do
    ARCHIVES+=("${archive}")
done < <(find "${PREFIX}" -type f -name '*.la' -print0)

if [ "${#ARCHIVES[@]}" -eq 0 ]; then
    exit 0
fi

rm -f -- "${ARCHIVES[@]}"

log info "Removed ${#ARCHIVES[@]} libtool archive(s) from ${PREFIX}"
