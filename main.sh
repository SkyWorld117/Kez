#!/usr/bin/env bash

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
                echo "  -v, --version    Show version information"
                echo "  -h, --help       Show this help message"
                echo "  init             Initialize the Fromager environment"
                echo "  selfcheck        Run self-checks on the Fromager installation"
                echo "  utilities        Manage utilities"
                echo "                   run `fgr utilities --help` for more information"
                echo "  cellar           Manage per application environments"
                echo "                   run `fgr cellar --help` for more information"
                echo "  install          Install a package"
                echo "                   run `fgr install --help` for more information"
                echo "  template         Fetch a template for an application"
                echo "                   run `fgr template --help` for more information"
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
                        echo "  add, create <cellar_name>       Create a new cellar named <cellar_name>"
                        echo "  rm, remove <cellar_name>        Remove an existing cellar named <cellar_name>"
                        echo "  ls, list                        List all existing cellars"
                        echo "  enter <cellar_name>             Load the environment of the specified cellar"
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
                        shift 3
                    ;;

                esac
                ;;

            install )
                # Install should create an environment and then install the package.

                case "$2" in

                    -h | --help )
                        fromager_info "Usage: fgr install [OPTIONS] [REQUIREMENTS_FILE] [CELLAR]"
                        echo "Options:"
                        echo "  -h, --help                                Show this help message"
                        echo "  -r, --read <requirements_file> [cellar]   Read requirements from <requirements_file> and install in [cellar]"
                        echo "                                            Notice the cellar argument should not be specified for compilers, "
                        echo "                                            MPIs, and vendor packages."
                        shift 2
                    ;;

                    -r | --read )
                        local requirements_file="$3"
                        if [ -z "$requirements_file" ] || [ ! -f "$requirements_file" ]; then
                            fromager_error "Invalid requirements file specified."
                            return 1
                        fi
                        local target_pkg="$(yq -r '.recipe.dependencies[0]' "$requirements_file")"
                        local pkg_type=$(yq -r '.cheese.type' "${FROMAGER_DB}/$target_pkg.yaml")
                        local pkg_version=$(yq -r ".cheese.${target_pkg}.version" "$requirements_file")
                        # Compilers, MPIs and vendor packages do not require a cellar.
                        # (In fact requires NOT to specify a cellar)
                        local cellar
                        if [ "$pkg_type" = "compiler" ]; then
                            cellar="${FROMAGER_ENV}/compilers/${target_pkg}-${pkg_version}"
                            fromager_install "$requirements_file" "$cellar"
                            shift 3
                        elif [ "$pkg_type" = "mpi" ]; then
                            local compiler_spec
                            local compiler="$(yq -r ".cheese.${target_pkg}.compiler" "$requirements_file")"
                            if [ "$compiler" = "system" ]; then
                                compiler_spec="system"
                            else
                                # Split at `@` (format: <compiler-name>@<compiler-version>)
                                local compiler_name=$(echo "$compiler" | cut -d'@' -f1)
                                local compiler_version=$(echo "$compiler" | cut -d'@' -f2)
                                compiler_spec="${FROMAGER_ENV}/compilers/${compiler_name}-${compiler_version}"
                            fi
                            cellar="${FROMAGER_ENV}/mpis/${target_pkg}-${pkg_version}-${compiler_spec}"
                            fromager_install "$requirements_file" "$cellar"
                            shift 3
                        elif [ "$pkg_type" = "vendor" ]; then
                            cellar="${FROMAGER_ENV}/vendors/${target_pkg}-${pkg_version}"
                            fromager_install "$requirements_file" "$cellar"
                            shift 3
                        else
                            # Require a cellar for other package types
                            if [ -z "$4" ] || [ ! -d "${FROMAGER_ENV}/$4" ]; then
                                fromager_error "Invalid cellar specified or not found."
                                return 1
                            fi
                            cellar="${FROMAGER_ENV}/$4"
                            fromager_install "$requirements_file" "$cellar"
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
                        echo "  -h, --help                             Show this help message"
                        echo "  <package>                              Generate the configuration template for the specified package"
                        echo "                                         without saving"
                        echo "  -s, --save <file name> <package>       Save the configuration template for the specified package"
                        shift 2
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