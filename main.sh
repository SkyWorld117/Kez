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

    while [ $# -gt 0 ]; do
        case "$1" in


            -v | --version )
                fromager_info "Fromager version 0.0.1"
                shift
                ;;


            -h | --help )
                fromager_info "Usage: fromager [OPTION]..."
                echo "Options:"
                format_option_help "-v, --version" "Show version information"
                format_option_help "-h, --help" "Show this help message"
                format_option_help "init" "Initialize the Fromager environment"
                format_option_help "selfcheck" "Run self-checks on the Fromager installation"
                format_option_help "utilities" "Manage utilities"
                format_option_help "" "run \`fgr utilities --help\` for more information"
                format_option_help "cellar" "Manage per application environments"
                format_option_help "" "run \`fgr cellar --help\` for more information"
                format_option_help "install" "Install a package"
                format_option_help "" "run \`fgr install --help\` for more information"
                format_option_help "template" "Fetch a template for an application"
                format_option_help "" "run \`fgr template --help\` for more information"
                shift
                ;;

            init )
                fromager_init
                shift
                ;;


            selfcheck )
                fromager_db_check
                shift
                ;;


            utilities )
                # Utilities for compiling, building, and managing the application.           

                case "$2" in

                    -h | --help )
                        echo "NOT IMPLEMENTED MODULE"
                        shift 2
                    ;;

                    add )
                        fromager_info "Adding utility: $3"
                        if [ -z "$3" ]; then
                            fromager_error "No utility specified to add."
                            return 1
                        fi
                        shift 3
                    ;;
                esac
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
                            unset FROMAGER_CELLAR
                            export PATH=$(echo "$PATH" | sed "s|${FROMAGER_ENV}/${FROMAGER_CELLAR}/bin| |g")
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

                esac
                ;;

            install )
                # Install should create an environment and then install the package.

                case "$2" in

                    -h | --help )
                        fromager_info "Usage: fgr install [OPTIONS] [CONFIG] [CELLAR]"
                        echo "Options:"
                        format_option_help "-h, --help" "Show this help message"
                        format_option_help "-r, --read <config> [cellar]" "Read requirements from <config> and install in [cellar]"
                        format_option_help "" "Notice the cellar argument should not be specified for compilers, MPIs, and vendor packages."
                        shift 2
                    ;;

                    -r | --read )
                        local config_file="$3"
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
                            fromager_install "$config_file" "$cellar"
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
                            fromager_install "$config_file" "$cellar"
                            shift 3
                        elif [ "$pkg_type" = "vendor" ]; then
                            cellar="${FROMAGER_ENV}/vendors/${target_pkg}-${pkg_version}"
                            fromager_install "$config_file" "$cellar"
                            shift 3
                        else
                            # Require a cellar for other package types
                            if [ -z "$4" ] || [ ! -d "${FROMAGER_ENV}/$4" ]; then
                                fromager_error "Invalid cellar specified or not found."
                                return 1
                            fi
                            cellar="${FROMAGER_ENV}/$4"
                            fromager_install "$config_file" "$cellar"
                            shift 4
                        fi
                    ;;

                    * )
                        fromager_info "Installing package: $2"
                        fromager_error "NOT IMPLEMENTED"
                        shift 2
                    ;;

                esac
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
                        format_option_help "parse <file>" "Parse the user configuration file <file>"
                        shift 2
                    ;;

                    parse )
                        fromager_info "Parsing template for: $3"
                        if [ -z "$3" ]; then
                            fromager_error "No package specified to parse."
                            return 1
                        fi
                        fromager_parser "$3" debug "${FROMAGER_WORKDIR}/.tmp"
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
                ;;

        
            * )
                fromager_error "Unknown option: $1"
                fromager_info "Use -h or --help for usage information."
                return 1
                ;;


        esac

    done

}