#!/bin/bash
# Bash completion for fgr (Fromager)

_fgr_complete() {
    local current_word="${COMP_WORDS[COMP_CWORD]}"
    local previous_word="${COMP_WORDS[COMP_CWORD-1]}"
    local command_word="${COMP_WORDS[1]}"
    local subcommand_word="${COMP_WORDS[2]}"

    # Helper function to get available cellars
    _get_cellars() {
        if [ -d "${FROMAGER_ENV}" ]; then
            ls "${FROMAGER_ENV}" 2>/dev/null | grep -v -E "^(system|utilities|compilers|mpis|vendors)$"
        fi
    }

    # Helper function to get available packages from database
    _get_packages() {
        if [ -d "${FROMAGER_DB}" ]; then
            ls "${FROMAGER_DB}"/*.yaml 2>/dev/null | xargs -I {} basename {} .yaml
        fi
    }

    # Main command completion
    if [ $COMP_CWORD -eq 1 ]; then
        local main_options="-v --version -h --help init selfcheck utilities cellar install template rt"
        COMPREPLY=($(compgen -W "${main_options}" -- "$current_word"))
        return 0
    fi

    # Subcommand completion based on main command
    case "$command_word" in
        utilities)
            case $COMP_CWORD in
                2)
                    local utilities_options="-h --help add empty"
                    COMPREPLY=($(compgen -W "${utilities_options}" -- "$current_word"))
                    ;;
                3)
                    case "$subcommand_word" in
                        add)
                            if [[ "$current_word" == -* ]]; then
                                COMPREPLY=($(compgen -W "-r --read" -- "$current_word"))
                            else
                                # Complete with package names
                                COMPREPLY=($(compgen -W "$(_get_packages)" -- "$current_word"))
                            fi
                            ;;
                    esac
                    ;;
                4)
                    case "$subcommand_word" in
                        add)
                            if [[ "${COMP_WORDS[3]}" == "-r" || "${COMP_WORDS[3]}" == "--read" ]]; then
                                # Complete with files
                                COMPREPLY=($(compgen -f -- "$current_word"))
                            else
                                # Complete with --config, --cellar options
                                COMPREPLY=($(compgen -W "--config --cellar" -- "$current_word"))
                            fi
                            ;;
                    esac
                    ;;
            esac
            ;;
        
        cellar)
            case $COMP_CWORD in
                2)
                    local cellar_options="-h --help add create rm remove ls list enter exit which empty"
                    COMPREPLY=($(compgen -W "${cellar_options}" -- "$current_word"))
                    ;;
                3)
                    case "$subcommand_word" in
                        add|create)
                            # No completion for new cellar name
                            COMPREPLY=()
                            ;;
                        rm|remove|enter|empty)
                            # Complete with existing cellar names
                            COMPREPLY=($(compgen -W "$(_get_cellars)" -- "$current_word"))
                            ;;
                        ls|list)
                            # No additional completion for list
                            COMPREPLY=()
                            ;;
                        which|exit)
                            # No additional completion for which or exit
                            COMPREPLY=()
                            ;;
                    esac
                    ;;
            esac
            ;;
        
        install)
            case $COMP_CWORD in
                2)
                    if [[ "$current_word" == -* ]]; then
                        local install_options="-h --help -r --read"
                        COMPREPLY=($(compgen -W "${install_options}" -- "$current_word"))
                    else
                        # Complete with package names
                        COMPREPLY=($(compgen -W "$(_get_packages)" -- "$current_word"))
                    fi
                    ;;
                3)
                    case "$subcommand_word" in
                        -r|--read)
                            # Complete with files
                            COMPREPLY=($(compgen -f -- "$current_word"))
                            ;;
                        *)
                            if [[ "$current_word" == -* ]]; then
                                COMPREPLY=($(compgen -W "--config --cellar" -- "$current_word"))
                            else
                                # For package installations, might be config values
                                COMPREPLY=()
                            fi
                            ;;
                    esac
                    ;;
                4)
                    case "$subcommand_word" in
                        -r|--read)
                            # Complete with cellar names for read command
                            COMPREPLY=($(compgen -W "$(_get_cellars)" -- "$current_word"))
                            ;;
                        *)
                            # Handle --cellar completion
                            if [[ "${COMP_WORDS[3]}" == "--cellar" ]]; then
                                COMPREPLY=($(compgen -W "$(_get_cellars)" -- "$current_word"))
                            elif [[ "${COMP_WORDS[2]}" == "--cellar" ]]; then
                                COMPREPLY=($(compgen -W "$(_get_cellars)" -- "$current_word"))
                            else
                                COMPREPLY=()
                            fi
                            ;;
                    esac
                    ;;
            esac
            ;;
        
        template)
            case $COMP_CWORD in
                2)
                    if [[ "$current_word" == -* ]]; then
                        local template_options="-h --help -s --save"
                        COMPREPLY=($(compgen -W "${template_options}" -- "$current_word"))
                    else
                        local template_commands="parse"
                        local packages="$(_get_packages)"
                        COMPREPLY=($(compgen -W "${template_commands} ${packages}" -- "$current_word"))
                    fi
                    ;;
                3)
                    case "$subcommand_word" in
                        parse)
                            # Complete with files
                            COMPREPLY=($(compgen -f -- "$current_word"))
                            ;;
                        -s|--save)
                            # Complete with files for save target
                            COMPREPLY=($(compgen -f -- "$current_word"))
                            ;;
                        *)
                            # No additional completion needed for package templates
                            COMPREPLY=()
                            ;;
                    esac
                    ;;
                4)
                    case "$subcommand_word" in
                        -s|--save)
                            # Complete with package names for save command
                            COMPREPLY=($(compgen -W "$(_get_packages)" -- "$current_word"))
                            ;;
                    esac
                    ;;
            esac
            ;;

        rt)
            case $COMP_CWORD in
                2)
                    local rt_options="-h --help factory build"
                    COMPREPLY=($(compgen -W "${rt_options}" -- "$current_word"))
                    ;;
                3)
                    case "$subcommand_word" in
                        factory)
                            local factory_options="add create rm remove ls list enter exit which cd"
                            COMPREPLY=($(compgen -W "${factory_options}" -- "$current_word"))
                            ;;
                        build)
                            # No additional options for build
                            COMPREPLY=()
                            ;;
                    esac
                    ;;
                4)
                    case "${COMP_WORDS[3]}" in
                        add|create)
                            # No completion for new factory name
                            COMPREPLY=()
                            ;;
                        rm|remove|enter)
                            # Complete with existing factory names
                            if [ -d "${FROMAGER_ENV}/factories" ]; then
                                COMPREPLY=($(compgen -W "$(ls ${FROMAGER_ENV}/factories 2>/dev/null)" -- "$current_word"))
                            else
                                COMPREPLY=()
                            fi
                            ;;
                        ls|list)
                            # No additional completion for list
                            COMPREPLY=()
                            ;;
                        which|exit)
                            # No additional completion for which or exit
                            COMPREPLY=()
                            ;;
                        cd)
                            # Complete with existing factory names for cd
                            if [ -d "${FROMAGER_ENV}/factories" ]; then
                                COMPREPLY=($(compgen -W "$(ls ${FROMAGER_ENV}/factories 2>/dev/null)" -- "$current_word"))
                            else
                                COMPREPLY=()
                            fi
                            ;;
                    esac
                    ;;
            esac
            ;;
        
        -v|--version|-h|--help|init|selfcheck)
            # These commands don't take additional arguments
            COMPREPLY=()
            ;;
        
        *)
            # Unknown command, no completion
            COMPREPLY=()
            ;;
    esac
}

# Register the completion function
complete -o filenames -F _fgr_complete fgr