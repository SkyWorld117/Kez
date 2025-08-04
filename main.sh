#!/usr/bin/env bash

fromager () {

    while [ $# -gt 0 ]; do
        case "$1" in


            -v | --version )
                echo "Fromager version 0.0.1"
                shift
                ;;


            -h | --help )
                echo -e "Usage: fromager [OPTION]..."
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
                        shift 2
                    ;;

                    add )
                        echo "Adding utility: $3"
                        if [ -z "$3" ]; then
                            echo "Error: No utility specified to add."
                            return 1
                        fi
                        shift 3
                    ;;
                esac
                ;;


            env )
                # Manage per application environments.

                case "$2" in

                    -h | --help )
                        shift 2
                    ;;

                    rm | remove )
                        echo "Removing environment: $3"
                        if [ -z "$3" ]; then
                            echo "Error: No environment specified to remove."
                            return 1
                        fi
                        if [ ! -d "${FROMAGER_ENV}/$3" ]; then
                            echo "Error: Environment '$3' does not exist."
                            return 1
                        fi
                        rm -rf "${FROMAGER_ENV}/$3"
                        shift 3
                    ;;

                    ls | list )
                        echo "Listing environments:"
                        if [ -d "${FROMAGER_ENV}" ]; then
                            echo "$(ls ${MS_TARG} | sed "s~Backups~~g" | xargs | sed "s~ ~\n~g" | nl -s '. ' -w 1)${NC}"
                        else
                            echo "No environments found."
                        fi
                        shift 2
                    ;;

                esac
                ;;


            install )
                # Install should create an environment and then install the package.

                case "$2" in

                    -h | --help )
                        echo "Install a package."
                        shift 2
                    ;;

                    -r | --read )
                        echo "Install a package from a requirements file."
                        shift 3
                    ;;

                    * )
                        echo "Installing package: $2"
                        shift 2
                    ;;

                esac
                ;;


            template )
                # Template gives a template for configuring the application and its environment.

                case "$2" in

                    -h | --help )
                        echo "Template for configuring the application."
                        shift 2
                    ;;

                    * )
                        echo "Generating template for: $2"
                        shift 2
                    ;;

                esac
                ;;

        
            * )
                echo "Unknown option: $1"
                echo "Use -h or --help for usage information."
                return 1
                ;;


        esac

    done

}