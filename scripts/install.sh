#!/usr/bin/env bash

set -Eeuo pipefail

KEZ_INFO=${KEZ_HOME}/bin/print_info
KEZ_WARNING=${KEZ_HOME}/print_warning
KEZ_ERROR=${KEZ_HOME}/bin/print_error
KEZ_SUCCESS=${KEZ_HOME}/bin/print_success

target_env=$1

force=false
if [ -n "${2:-}" ] && [ "$2" == "--force" ]; then
    force=true
fi

rm -rf $target_env/.tmp/source $target_env/.tmp/source*

# Process each package and its instructions
package_count=$(yq -r '. | length' "$target_env/.tmp/ins.yaml")

# Create state.yaml to store all the installed packages if it doesn't exist
if [ ! -f "$target_env/state.yaml" ] || ! $(yq -r ". | has(\"state\")" "$target_env/state.yaml") || [ "$force" == true ]; then
    echo "state:" > "$target_env/state.yaml"
fi

for ((pkg_idx=0; pkg_idx<package_count; pkg_idx++)); do
    # Extract package name
    package_name=$(yq -r ".[$pkg_idx].package" "$target_env/.tmp/ins.yaml")

    # Skip if package is already installed (unless --force is used)
    if $(grep -q "  - $package_name" "$target_env/state.yaml") && [ "$force" == false ]; then
        ${KEZ_WARNING} "Package $package_name is already installed. Skipping."
        continue
    fi

    ${KEZ_INFO} "Processing package: $package_name"
    cd "$target_env/.tmp"

    # Get number of instructions for this package
    instruction_count=$(yq -r ".[$pkg_idx].instructions | length" "$target_env/.tmp/ins.yaml")

    # Execute each instruction
    for ((instr_idx=0; instr_idx<instruction_count; instr_idx++)); do
        instruction=$(yq -r ".[$pkg_idx].instructions[$instr_idx]" "$target_env/.tmp/ins.yaml")
        ${KEZ_INFO} "Executing: $instruction"
        saved_pkg_idx=$pkg_idx
        saved_instr_idx=$instr_idx
        eval "$instruction"
        instruction_status=$?
        pkg_idx=$saved_pkg_idx
        instr_idx=$saved_instr_idx
        if [ $instruction_status -ne 0 ]; then
            ${KEZ_ERROR} "Failed to execute: $instruction"
            exit 1
        fi
    done

    # Mark package as installed
    echo "  - $package_name" >> "$target_env/state.yaml"

    ${KEZ_SUCCESS} "Completed processing package: $package_name"

    rm -rf "$target_env/.tmp/source"
done

# If a module file is not created in the modulefiles directory, create a module file for the cellar
modulefiles_dir=${KEZ_WORKDIR}/$(yq -r '.fromager.paths.modulefiles' "${KEZ_WORKDIR}/config.yaml")
if [ ! -f "$modulefiles_dir/$(basename $target_env)" ]; then
    ${KEZ_INFO} "Creating module file for cellar: $target_env in $modulefiles_dir"
    cat <<EOF > "$modulefiles_dir/$(basename $target_env)"
#%Module1.0
proc ModulesHelp { } {
    puts stderr "This module loads the $(basename $target_env) cellar environment."
}
module-whatis "Loads the $(basename $target_env) cellar environment"
setenv KEZ_ENV "$(basename $target_env)"
prepend-path PATH "$target_env/bin"
EOF
    ${KEZ_SUCCESS} "Module file created for cellar: $target_env in $modulefiles_dir"
else
    ${KEZ_INFO} "Module file already exists for cellar: $target_env in $modulefiles_dir. Skipping creation."
fi

set +Eeuo pipefail
