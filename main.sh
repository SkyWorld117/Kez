#!/usr/bin/env bash

# Format option and help text with dynamic wrapping and proper alignment
format_option_help() {
    local option="$1"
    local help_text="$2"
    local base_indent="  "
    local option_width=25  # Standard width for option column
    local width=$(tput cols 2>/dev/null) || width=80
    local min_help_width=30
    local min_option_width=15
    
    # Calculate available space for help text
    local total_prefix_width=$((${#base_indent} + option_width))
    local help_width=$((width - total_prefix_width))
    local option_content_width=$((width - ${#base_indent}))
    
    # Check if option itself is too long for the allocated space
    local option_too_long=false
    if [ "${#option}" -gt "$option_width" ] && [ "$option_content_width" -gt "$min_option_width" ]; then
        option_too_long=true
    fi
    
    # Use stacked format if:
    # - Terminal is too narrow for side-by-side layout
    # - Help text is very long
    # - Option text is too long for the allocated space
    if [ "$help_width" -lt "$min_help_width" ] || [ "${#help_text}" -gt $((help_width * 2)) ] || [ "$option_too_long" = true ]; then
        # Stacked format: option on one line, help text on next line with extra indent
        local help_indent="${base_indent}    "  # Extra indentation for help text
        local help_content_width=$((width - ${#help_indent}))
        
        # Ensure minimum widths
        if [ "$help_content_width" -lt 20 ]; then
            help_content_width=20
        fi
        if [ "$option_content_width" -lt 15 ]; then
            option_content_width=15
        fi
        
        # Wrap and output option if it's too long
        if [ "${#option}" -gt "$option_content_width" ]; then
            echo "$option" | fold -s -w "$option_content_width" | sed "s/^/$base_indent/"
        else
            echo "${base_indent}${option}"
        fi
        
        # Wrap and output help text
        if [ -n "$help_text" ]; then
            echo "$help_text" | fold -s -w "$help_content_width" | sed "s/^/$help_indent/"
        fi
    else
        # Side-by-side format: option and help text on same line
        local help_alignment_spaces=""
        local current_option_width=${#option}
        
        # Create alignment spaces
        if [ "$current_option_width" -lt "$option_width" ]; then
            local spaces_needed=$((option_width - current_option_width))
            help_alignment_spaces=$(printf "%*s" "$spaces_needed" "")
        fi
        
        # Format help text with proper wrapping
        local wrapped_help
        if [ "${#help_text}" -gt "$help_width" ]; then
            # Wrap help text and align continuation lines
            local continuation_indent="${base_indent}$(printf "%*s" "$option_width" "")"
            wrapped_help=$(echo "$help_text" | fold -s -w "$help_width" | sed "1!s/^/$continuation_indent/")
        else
            wrapped_help="$help_text"
        fi
        
        # Output the formatted line
        echo "${base_indent}${option}${help_alignment_spaces}${wrapped_help}"
    fi
}

fgr () {
    # Show help if no arguments provided
    if [[ $# -eq 0 ]]; then
        set -- "-h"
    fi

    while [ $# -gt 0 ]; do
        case "$1" in


            -v | --version )
                fromager_info "Fromager version 0.0.1"
                shift
                break
                ;;


            -h | --help )
                fromager_info "Usage: fromager [OPTION]..."
                echo "Options:"
                format_option_help "-v, --version" "Show version information"
                format_option_help "-h, --help" "Show this help message"
                format_option_help "init" "Initialize the Fromager environment"
                format_option_help "" "run \`fgr init --help\` for more information"
                format_option_help "selfcheck" "Run self-checks on the Fromager installation"
                format_option_help "utilities" "Manage utilities"
                format_option_help "" "run \`fgr utilities --help\` for more information"
                format_option_help "cellar" "Manage per application environments"
                format_option_help "" "run \`fgr cellar --help\` for more information"
                format_option_help "install" "Install a package"
                format_option_help "" "run \`fgr install --help\` for more information"
                format_option_help "template" "Fetch a template for an application"
                format_option_help "" "run \`fgr template --help\` for more information"
                format_option_help "rt" "Manage rapid test factories"
                format_option_help "" "run \`fgr rt --help\` for more information"
                shift
                break
                ;;

            init )
                if [ -n "${2:-}" ]; then
                    case "$2" in
                        -h | --help )
                            fromager_info "Usage: fgr init [OPTION]"
                            echo "Options:"
                            format_option_help "-h, --help" "Show this help message"
                            format_option_help "--refresh" "Refresh the Fromager environment by reinstalling core utilities"
                            shift 2
                            return 0
                            ;;

                        --refresh )
                            fromager_init --refresh
                            shift 2
                            ;;

                        * )
                            fromager_error "Unknown init option: $2"
                            fromager_info "Use -h or --help for usage information."
                            return 1
                            ;;

                    esac
                else
                    fromager_init
                    shift 1
                fi
                break
                ;;


            selfcheck )
                fromager_db_check
                shift 1
                break
                ;;


            utilities )
                # Utilities for compiling, building, and managing the application.           

                case "$2" in

                    -h | --help )
                        fromager_info "Usage: fgr utilities [OPTION] [ARGUMENTS]"
                        echo "Options:"
                        format_option_help "-h, --help" "Show this help message"
                        format_option_help "add <utility>" "Add a utility to the utilities cellar"
                        format_option_help "add -r, --read <file>" "Read a configuration file and add all utilities specified"
                        format_option_help "empty" "Remove all utilities"
                        shift 2
                        ;;

                    add )

                        case "$3" in

                            -r | --read )
                                if [ -z "$4"]; then
                                    fromager_error "No utilities file specified to read."
                                    return 1
                                fi
                                fromager_install "$4" "${FROMAGER_ENV}/utilities"
                                shift 4
                                ;;

                            * )
                                fromager_info "Adding utility: $3"
                                if [ -z "$3" ]; then
                                    fromager_error "No utility specified to add."
                                    return 1
                                fi
                                shift 2
                                local cellar=$(fromager_cmdline_parser "$@" --cellar utilities | tail -n 1)
                                fromager_cmdline_install "$cellar"
                                shift "$#"
                                ;;

                        esac

                        ;;

                empty )
                    rm -rf "${FROMAGER_ENV}/utilities"/*
                    fromager_success "All utilities removed."
                    shift 2
                    ;;

                esac
                break
                ;;


            cellar )
                # Manage per application environments.

                case "$2" in

                    -h | --help )
                        fromager_info "Usage: fgr cellar [OPTION] [ARGUMENTS]"
                        echo "Options:"
                        format_option_help "add, create <cellar>" "Create a new cellar named <cellar>"
                        format_option_help "rm, remove <cellar>" "Remove an existing cellar named <cellar>"
                        format_option_help "ls, list" "List all existing cellars"
                        format_option_help "enter <cellar>" "Load the environment of <cellar>"
                        format_option_help "exit" "Exit the current cellar"
                        format_option_help "which" "Show the current cellar"
                        shift 2
                        ;;

                    add | create )
                        fromager_info "Creating new cellar: $3"
                        if [ -z "$3" ]; then
                            fromager_error "No cellar specified to create."
                            return 1
                        fi
                        mkdir -p "${FROMAGER_ENV}/$3"
                        fromager_success "Cellar '$3' created successfully."
                        shift 3
                        ;;

                    rm | remove )
                        fromager_info "Removing cellar: $3"
                        if [ -z "$3" ]; then
                            fromager_error "No cellar specified to remove."
                            return 1
                        fi
                        if [ ! -d "${FROMAGER_ENV}/$3" ]; then
                            fromager_error "Cellar '$3' does not exist."
                            return 1
                        fi
                        if [ ! -z "${FROMAGER_CELLAR:-}" ] && [ "${FROMAGER_CELLAR}" = "$3" ]; then
                            fromager_error "Cannot remove the currently active cellar '$3'. Please exit it first."
                            return 1
                        fi
                        rm -rf "${FROMAGER_ENV}/$3"
                        shift 3
                        ;;

                    ls | list )
                        fromager_info "Listing cellars:"
                        local cellars
                        if [ -d "${FROMAGER_ENV}" ]; then
                            cellars="$(ls ${FROMAGER_ENV} | sed "s~system~~g" | sed "s~utilities~~g" | sed "s~compilers~~g" | sed "s~mpis~~g" | sed "s~vendors~~g" | xargs | sed "s~ ~\n~g" | nl -s '. ' -w 1)"
                        fi
                        # Check if cellars is empty or only contains whitespace/newline etc.
                        if [ -z "${cellars// }" ]; then
                            fromager_info "No cellars found."
                        else
                            echo "$cellars"
                        fi
                        shift 2
                        ;;

                    enter )
                        fromager_info "Entering cellar: $3"
                        if [ -z "$3" ] || [ ! -d "${FROMAGER_ENV}/$3" ]; then
                            fromager_error "Invalid cellar specified."
                            return 1
                        fi
                        if [ -z "${FROMAGER_CELLAR:-}" ]; then
                            export FROMAGER_CELLAR="$3"
                            export PATH="${FROMAGER_ENV}/${FROMAGER_CELLAR}/bin:$PATH"
                        else
                            fromager_error "Already in the cellar ${FROMAGER_CELLAR}, please exit first."
                            return 1
                        fi
                        shift 3
                        ;;

                    exit )
                        fromager_info "Exiting cellar: ${FROMAGER_CELLAR}"
                        if [ -n "${FROMAGER_CELLAR:-}" ]; then
                            export PATH=$(echo "$PATH" | sed "s|${FROMAGER_ENV}/${FROMAGER_CELLAR}/bin:||g")
                            unset FROMAGER_CELLAR
                        else
                            fromager_error "Not currently in a cellar."
                            return 1
                        fi
                        shift 2
                        ;;

                    which )
                        if [ -z "${FROMAGER_CELLAR:-}" ]; then
                            fromager_info "Not currently in a cellar."
                        else
                            fromager_info "You are currently in the cellar: ${FROMAGER_CELLAR}"
                        fi
                        shift 2
                        ;;

                    empty )
                        fromager_info "Emptying cellar: $3"
                        if [ -z "$3" ]; then
                            fromager_error "No cellar specified to empty."
                            return 1
                        fi
                        if [ ! -d "${FROMAGER_ENV}/$3" ]; then
                            fromager_error "Cellar '$3' does not exist."
                            return 1
                        fi
                        rm -rf "${FROMAGER_ENV}/$3"/*
                        rm -rf "${FROMAGER_ENV}/$3"/.tmp
                        fromager_success "Cellar '$3' emptied successfully."
                        shift 3
                        ;;

                    * )
                        fromager_error "Unknown cellar command: $2"
                        fromager_info "Use -h or --help for usage information."
                        return 1
                        shift 2
                        ;;

                esac
                break
                ;;


            load-mpi )
                if [ -z "$2" ]; then
                    fromager_error "No MPI specified to load."
                    return 1
                fi
                if [ -z "${FROMAGER_ENV}/mpis/$2" ] || [ ! -d "${FROMAGER_ENV}/mpis/$2" ]; then
                    fromager_error "MPI '$2' not found in the environment."
                    return 1
                fi
                if [ -n "${FROMAGER_MPI:-}" ]; then
                    fromager_warning "An MPI (${FROMAGER_MPI}) is already loaded. Overwriting with $2."
                    export PATH=$(echo "$PATH" | sed "s|${FROMAGER_ENV}/mpis/${FROMAGER_MPI}/bin:||g")
                fi
                export FROMAGER_MPI="$2"
                export PATH="${FROMAGER_ENV}/mpis/${FROMAGER_MPI}/bin:$PATH"
                fromager_success "MPI '$2' loaded successfully."
                shift 2
                break
                ;;


            unload-mpi )
                if [ -z "${FROMAGER_MPI:-}" ]; then
                    fromager_error "No MPI is currently loaded."
                    return 1
                fi
                fromager_info "Unloading MPI: ${FROMAGER_MPI}"
                export PATH=$(echo "$PATH" | sed "s|${FROMAGER_ENV}/mpis/${FROMAGER_MPI}/bin:||g")
                unset FROMAGER_MPI
                fromager_success "MPI unloaded successfully."
                shift 1
                break
                ;;


            load-compiler )
                if [ -z "$2" ]; then
                    fromager_error "No compiler specified to load."
                    return 1
                fi
                if [ -z "${FROMAGER_ENV}/compilers/$2" ] || [ ! -d "${FROMAGER_ENV}/compilers/$2" ]; then
                    fromager_error "Compiler '$2' not found in the environment."
                    return 1
                fi
                if [ -n "${FROMAGER_COMPILER:-}" ]; then
                    fromager_warning "A compiler (${FROMAGER_COMPILER}) is already loaded. Overwriting with $2."
                    export PATH=$(echo "$PATH" | sed "s|${FROMAGER_ENV}/compilers/${FROMAGER_COMPILER}/bin:||g")
                fi
                export FROMAGER_COMPILER="$2"
                export PATH="${FROMAGER_ENV}/compilers/${FROMAGER_COMPILER}/bin:$PATH"
                fromager_success "Compiler '$2' loaded successfully."
                shift 2
                break
                ;;


            unload-compiler )
                if [ -z "${FROMAGER_COMPILER:-}" ]; then
                    fromager_error "No compiler is currently loaded."
                    return 1
                fi
                fromager_info "Unloading compiler: ${FROMAGER_COMPILER}"
                export PATH=$(echo "$PATH" | sed "s|${FROMAGER_ENV}/compilers/${FROMAGER_COMPILER}/bin:||g")
                unset FROMAGER_COMPILER
                fromager_success "Compiler unloaded successfully."
                shift 1
                break
                ;;


            install )
                # Install should create an environment and then install the package.

                case "$2" in

                    -h | --help )
                        fromager_info "Usage: fgr install [OPTION] [ARGUMENTS]"
                        echo "Options:"
                        format_option_help "<pkg> [--config [configs]] [--cellar <cellar>]" "Install <pkg> with [configs] and in [cellar]."
                        format_option_help "" "Notice the cellar argument should not be specified for compilers, MPIs, and vendor packages."
                        format_option_help "" "If no cellar is specified, the FROMAGER_CELLAR environment variable is used if set."
                        format_option_help "-h, --help" "Show this help message"
                        format_option_help "-r, --read <config> [cellar]" "Read requirements from <config> and install in [cellar]"
                        format_option_help "" "Notice the cellar argument should not be specified for compilers, MPIs, and vendor packages."
                        format_option_help "" "If no cellar is specified, the FROMAGER_CELLAR environment variable is used if set."
                        shift 2
                        ;;

                    -r | --read )
                        local config_file="$3"
                        config_file=$(realpath "$config_file")
                        if [ -z "$config_file" ] || [ ! -f "$config_file" ]; then
                            fromager_error "Invalid requirements file specified."
                            return 1
                        fi
                        local target_pkg="$(yq -r '.recipe.dependencies[0]' "$config_file")"
                        local pkg_type=$(yq -r '.cheese.type' "${FROMAGER_DB}/$target_pkg.yaml")
                        local pkg_version=$(yq -r ".cheese.${target_pkg}.version" "$config_file")
                        # Compilers, MPIs and vendor packages do not require a cellar.
                        # (In fact requires NOT to specify a cellar)
                        local cellar
                        if [ "$pkg_type" = "compiler" ]; then
                            cellar="${FROMAGER_ENV}/compilers/${target_pkg}-${pkg_version}"
                            shift 3
                        elif [ "$pkg_type" = "mpi" ]; then
                            local compiler_spec
                            local compiler="$(yq -r ".cheese.${target_pkg}.compiler" "$config_file")"
                            if [ "$compiler" = "system" ]; then
                                compiler_spec="system"
                            else
                                # Split at `@` (format: <compiler-name>@<compiler-version>)
                                local compiler_name=$(echo "$compiler" | cut -d'@' -f1)
                                local compiler_version=$(echo "$compiler" | cut -d'@' -f2)
                                compiler_spec="${FROMAGER_ENV}/compilers/${compiler_name}-${compiler_version}"
                            fi
                            cellar="${FROMAGER_ENV}/mpis/${target_pkg}-${pkg_version}-${compiler_spec}"
                            shift 3
                        elif [ "$pkg_type" = "vendor" ]; then
                            cellar="${FROMAGER_ENV}/vendors/${target_pkg}-${pkg_version}"
                            shift 3
                        else
                            # Require a cellar for other package types
                            # If no cellar specified in command line but FROMAGER_CELLAR is set, use that.
                            if [ -z "$4" ] && [ -z "${FROMAGER_CELLAR:-}" ]; then
                                fromager_error "No cellar specified to install the package into."
                                return 1
                            else
                                if [ -n "$4" ]; then
                                    cellar="${FROMAGER_ENV}/$4"
                                    shift 4
                                else
                                    cellar="${FROMAGER_ENV}/${FROMAGER_CELLAR}"
                                    shift 3
                                fi
                            fi
                            if [ ! -d "$cellar" ]; then
                                fromager_error "Cellar '$cellar' does not exist."
                                return 1
                            fi
                        fi
                        fromager_info "Using $user_config for user configuration and $cellar as target cellar."
                        fromager_parser "$config_file" release "$cellar" > /dev/null
                        fromager_install "$cellar"
                        ;;

                    * )
                        fromager_info "Installing package: $2"
                        shift 1
                        local cellar="$(fromager_cmdline_parser "$@" | tail -n 1)"
                        fromager_install "$cellar"
                        shift "$#"
                        ;;

                esac
                break
                ;;


            template )
                # Template gives a template for configuring the application and its environment.

                case "$2" in

                    -h | --help )
                        fromager_info "Usage: fgr template [OPTION] [ARGUMENTS]"
                        echo "Options:"
                        format_option_help "-h, --help" "Show this help message"
                        format_option_help "<package>" "Generate the configuration template for the specified package without saving"
                        format_option_help "-s, --save <file> <package>" "Save the configuration template for the specified package"
                        format_option_help "parse <file>" "Parse the user configuration file <file> for debugging, cellar path is set to FROMAGER_WORKDIR/.tmp"
                        shift 2
                        ;;

                    parse )
                        fromager_info "Parsing template for: $3"
                        if [ -z "$3" ]; then
                            fromager_error "No package specified to parse."
                            return 1
                        fi
                        fromager_parser "$3" release "${FROMAGER_WORKDIR}/.tmp"
                        shift 3
                        ;;

                    -s | --save )
                        fromager_info "Saving template for: $3"
                        if [ -z "$3" ]; then
                            fromager_error "No template name specified."
                            return 1
                        fi
                        if [ -z "$4" ]; then
                            fromager_error "No package specified."
                            return 1
                        fi
                        fromager_user_config_gen "$4" "$3"
                        shift 4
                        ;;

                    * )
                        fromager_info "Generating template for: $2"
                        fromager_user_config_gen "$2"
                        shift 2
                        ;;

                esac
                break
                ;;

        
            rt )
                # RT (rapid test) is used for batch building and running tests on packages.

                case "$2" in

                    -h | --help )
                        fromager_info "Usage: fgr rt [OPTION] [ARGUMENTS]"
                        echo "Options:"
                        format_option_help "-h, --help" "Show this help message"
                        format_option_help "factory add, create <factory>" "Create a new factory named <factory>"
                        format_option_help "factory rm, remove <factory>" "Remove an existing factory named <factory>"
                        format_option_help "factory ls, list" "List all existing factories"
                        format_option_help "factory enter <factory>" "Load the environment of <factory>"
                        format_option_help "factory exit" "Exit the current factory"
                        format_option_help "factory which" "Show the current factory"
                        format_option_help "build" "Build all configurations in the current factory"
                        format_option_help "taste" "Run the rapid test profile in the current factory"
                        format_option_help "summarize" "Summarize the results of the rapid tests"
                        format_option_help "try <config> <package>" "Isolate and install <package> and its dependencies in the current cellar for testing"
                        shift 2
                        ;;

                    factory )
                        case "$3" in

                            add | create )
                                fromager_info "Creating new factory: $4"
                                if [ -z "$4" ]; then
                                    fromager_error "No factory specified to create."
                                    return 1
                                fi
                                mkdir -p "${FROMAGER_WORKDIR}/factories/$4"
                                # Create cheese wheels (user configs) directory
                                mkdir -p "${FROMAGER_WORKDIR}/factories/$4/wheels"
                                # Create tasting rooms (test results) directory
                                mkdir -p "${FROMAGER_WORKDIR}/factories/$4/tasting_rooms"
                                fromager_success "Factory '$4' created successfully."
                                shift 4
                                ;;

                            rm | remove )
                                fromager_info "Removing factory: $4"
                                if [ -z "$4" ]; then
                                    fromager_error "No factory specified to remove."
                                    return 1
                                fi
                                if [ ! -d "${FROMAGER_WORKDIR}/factories/$4" ]; then
                                    fromager_error "Factory '$4' does not exist."
                                    return 1
                                fi
                                rm -rf "${FROMAGER_WORKDIR}/factories/$4"
                                shift 4
                                ;;

                            ls | list )
                                fromager_info "Listing factories:"
                                local factories
                                if [ -d "${FROMAGER_WORKDIR}/factories" ]; then
                                    factories="$(ls ${FROMAGER_WORKDIR}/factories | xargs | sed "s~ ~\n~g" | nl -s '. ' -w 1)"
                                fi
                                # Check if factories is empty or only contains whitespace/newline etc.
                                if [ -z "${factories// }" ]; then
                                    fromager_info "No factories found."
                                else
                                    echo "$factories"
                                fi
                                shift 3
                                ;;

                            enter )
                                fromager_info "Entering factory: $4"
                                if [ -z "$4" ] || [ ! -d "${FROMAGER_WORKDIR}/factories/$4" ]; then
                                    fromager_error "Invalid factory specified."
                                    return 1
                                fi
                                if [ -z "${FROMAGER_FACTORY:-}" ]; then
                                    export FROMAGER_FACTORY="$4"
                                else
                                    fromager_error "Already in the factory ${FROMAGER_FACTORY}, please exit first."
                                    return 1
                                fi
                                shift 4
                                ;;

                            exit )
                                fromager_info "Exiting factory: ${FROMAGER_FACTORY}"
                                if [ -n "${FROMAGER_FACTORY:-}" ]; then
                                    unset FROMAGER_FACTORY
                                else
                                    fromager_error "Not currently in a factory."
                                    return 1
                                fi
                                shift 3
                                ;;

                            which )
                                if [ -z "${FROMAGER_FACTORY:-}" ]; then
                                    fromager_info "Not currently in a factory."
                                else
                                    fromager_info "You are currently in the factory: ${FROMAGER_FACTORY}"
                                fi
                                shift 3
                                ;;

                            cd )
                                if [ -z "$4" ]; then
                                    fromager_error "No directory specified to change to."
                                    return 1
                                fi
                                if [ ! -d "${FROMAGER_WORKDIR}/factories/$4" ]; then
                                    fromager_error "Factory '$4' does not exist."
                                    return 1
                                fi
                                cd "${FROMAGER_WORKDIR}/factories/$4"
                                fromager_success "Changed directory to factory '$4'."
                                shift 4
                                ;;

                            * )
                                fromager_error "Unknown factory command: $3"
                                fromager_info "Use -h or --help for usage information."
                                return 1
                                shift 3
                                ;;

                        esac
                        ;;

                    build )
                        fromager_info "Starting rapid test build process..."
                        if [ -z "${FROMAGER_FACTORY:-}" ]; then
                            fromager_error "Not currently in a factory. Please enter a factory first."
                            return 1
                        fi
                        # Install all configurations in the factory's wheels directory
                        local wheels_dir="${FROMAGER_WORKDIR}/factories/${FROMAGER_FACTORY}/wheels"
                        if [ ! -d "$wheels_dir" ]; then
                            fromager_error "Wheels directory not found in the current factory."
                            return 1
                        fi
                        local config_files=("$wheels_dir"/*.yaml)
                        if [ ${#config_files[@]} -eq 0 ]; then
                            fromager_error "No configuration files found in the wheels directory."
                            return 1
                        fi
                        for config in "${config_files[@]}"; do
                            if [ -f "$config" ]; then
                                fromager_info "Installing from configuration: $config"
                                local target_cellar="${FROMAGER_WORKDIR}/factories/${FROMAGER_FACTORY}/cellars/$(basename "$config" .yaml)"
                                mkdir -p "$target_cellar"
                                fromager_install "$config" "$target_cellar"
                                fromager_success "Installation from $config completed."
                            fi
                        done
                        fromager_success "Rapid test build process completed."
                        shift 2
                        ;;

                    taste )
                        fromager_info "Tasting the cheese..."
                        if [ -z "${FROMAGER_FACTORY:-}" ]; then
                            fromager_error "Not currently in a factory. Please enter a factory first."
                            return 1
                        fi
                        # Check if a configuration exists for the tasting
                        local tasting_room="${FROMAGER_WORKDIR}/factories/${FROMAGER_FACTORY}/tasting_rooms"
                        if [ ! -f "${tasting_room}/config.yaml" ]; then
                            fromager_error "No tasting configuration found in the tasting room."
                            return 1
                        fi
                        if ! fromager_rt_profile; then
                            fromager_error "Tasting profile execution failed."
                            return 1
                        fi
                        fromager_success "Tasting completed."
                        shift 2
                        ;;

                    summarize )
                        fromager_info "Summarizing tasting results..."
                        if [ -z "${FROMAGER_FACTORY:-}" ]; then
                            fromager_error "Not currently in a factory. Please enter a factory first."
                            return 1
                        fi
                        local tasting_room="${FROMAGER_WORKDIR}/factories/${FROMAGER_FACTORY}/tasting_rooms"
                        if [ ! -d "$tasting_room" ]; then
                            fromager_error "Tasting room directory not found."
                            return 1
                        fi
                        if ! fromager_rt_summarize; then
                            fromager_error "Tasting summary failed."
                            return 1
                        fi
                        fromager_success "Tasting summary completed."
                        shift 2
                        ;;


                    try )
                        # Isolate all the packages in a cellar for testing and rebuilding.
                        # Must takes a user config and a target package name.
                        # User must be in a cellar.
                        if [ -z "${FROMAGER_CELLAR:-}" ]; then
                            fromager_error "Not currently in a cellar. Please enter a cellar first."
                            return 1
                        fi
                        if [ -z "$3" ] || [ ! -f "$3" ]; then
                            fromager_error "No user configuration file specified or file does not exist."
                            return 1
                        fi
                        if [ -z "$4" ]; then
                            fromager_error "No target package specified."
                            return 1
                        fi
                        fromager_parser "$3" debug "${FROMAGER_ENV}/${FROMAGER_CELLAR}" > /dev/null
                        fromager_rt_install "${FROMAGER_ENV}/${FROMAGER_CELLAR}" "$3" "$4"
                        fromager_success "Rapid test try completed."
                        shift 4
                        ;;

                    * )
                        fromager_error "Unknown rt command: $2"
                        fromager_info "Use -h or --help for usage information."
                        return 1
                        shift 2
                        ;;

                esac
                break
                ;;


            * )
                fromager_error "Unknown option: $1"
                fromager_info "Use -h or --help for usage information."
                return 1
                ;;


        esac

    done

    if [ $# -gt 0 ]; then
        fromager_error "Too many arguments"
        fromager_warning "Arguments starting with $1 are ignored."
        fromager_info "Use -h or --help for usage information."
        return 1
    fi

}
