#!/usr/bin/env bash
set -euo pipefail

project_directory="$(cd -- "${BASH_SOURCE[0]%/*}/.." && /usr/bin/pwd -P)"
home_directory="${HOME:?HOME must be set}"
config_home="${XDG_CONFIG_HOME:-$home_directory/.config}"
install_prefix="$home_directory/.local"
config_directory="$config_home/dino"
apps_list="$config_home/apps.list"
build_directory="$project_directory/build"
desktop_id="io.github.dino_runner"
dry_run=0
standalone=0
temporary_paths=()

usage() {
    /usr/bin/printf '%s\n' \
        'Usage: install-desktop.sh [--dry-run] [--standalone]' \
        '       [--prefix DIR] [--config-dir DIR] [--apps-list FILE] [--build-dir DIR]' \
        '' \
        'Install the Dino executable, icon, and desktop entry.' \
        'Paths must be absolute. --standalone skips apps.list and active-theme following.'
}

fail() {
    /usr/bin/printf 'install-desktop: %s\n' "$*" >&2
    exit 1
}

cleanup_temporary_paths() {
    local path
    for path in "${temporary_paths[@]}"; do
        if [[ -e "$path" || -L "$path" ]]; then
            /usr/bin/rm -f -- "$path"
        fi
    done
}

trap cleanup_temporary_paths EXIT

normalize_path() {
    if [[ "$1" == / ]]; then
        REPLY=/
    else
        REPLY="${1%/}"
    fi
}

validate_option_path() {
    local option="$1"
    local path="$2"
    [[ "$path" == /* ]] || fail "$option requires an absolute path"
    [[ "$path" != *$'\n'* && "$path" != *'"'* && "$path" != *'%'* \
        && "$path" == "${path//\\/}" && "$path" != *'|'* ]] \
        || fail "$option contains an unsupported character"
}

parse_options() {
    while (( $# )); do
        case "$1" in
            --dry-run)
                dry_run=1
                shift
                ;;
            --standalone)
                standalone=1
                shift
                ;;
            --help|-h)
                usage
                exit 0
                ;;
            --prefix|--config-dir|--apps-list|--build-dir)
                (( $# >= 2 )) || fail "Missing value for $1"
                validate_option_path "$1" "$2"
                case "$1" in
                    --prefix)
                        normalize_path "$2"
                        install_prefix="$REPLY"
                        ;;
                    --config-dir)
                        normalize_path "$2"
                        config_directory="$REPLY"
                        ;;
                    --apps-list)
                        apps_list="$2"
                        ;;
                    --build-dir)
                        normalize_path "$2"
                        build_directory="$REPLY"
                        ;;
                esac
                shift 2
                ;;
            *)
                fail "Unknown option: $1"
                ;;
        esac
    done
}

guard_directory() {
    local path="$1"
    [[ ! -L "$path" || -d "$path" ]] || fail "Directory symlink has no directory target: $path"
    [[ ! -e "$path" || -d "$path" ]] || fail "Expected a directory: $path"
}

guard_regular_or_missing() {
    local path="$1"
    [[ ! -L "$path" ]] || fail "Refusing to replace a symlink: $path"
    [[ ! -e "$path" || -f "$path" ]] || fail "Expected a regular file: $path"
}

validate_installation() {
    [[ -x "$build_directory/dino" ]] || fail "Build the game first: $build_directory/dino"
    [[ "$install_prefix" != / && -n "$install_prefix" ]] || fail 'Invalid installation prefix'
    [[ -n "$config_directory" && -n "$apps_list" && -n "$build_directory" ]] || fail 'Paths cannot be empty'

    local binary="$install_prefix/bin/dino"
    local desktop="$install_prefix/bin/applications/$desktop_id.desktop"
    local icon="$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg"
    local registered="$install_prefix/share/applications/$desktop_id.desktop"
    local theme_file="$config_directory/theme.json"
    local license="$install_prefix/share/licenses/dino/omarchy-dino-MIT.txt"
    local managed_file

    if (( ! standalone )); then
        [[ "$apps_list" != "$install_prefix" && "$apps_list" != "$install_prefix/"* \
            && "$install_prefix" != "$apps_list/"* ]] \
            || fail 'The app list must be outside the install prefix'
        for managed_file in "$binary" "$desktop" "$icon" "$registered" "$license" "$theme_file"; do
            [[ "$apps_list" != "$managed_file" ]] || fail 'The app list conflicts with an install file'
        done
    fi
    for managed_file in "$binary" "$desktop" "$icon" "$registered" "$license"; do
        [[ "$config_directory" != "$managed_file" ]] || fail 'The config directory conflicts with an install file'
    done

    guard_directory "$install_prefix"
    guard_directory "$install_prefix/bin"
    guard_directory "$install_prefix/bin/applications"
    guard_directory "$install_prefix/share"
    guard_directory "$install_prefix/share/applications"
    guard_directory "$install_prefix/share/icons"
    guard_directory "$install_prefix/share/icons/hicolor"
    guard_directory "$install_prefix/share/icons/hicolor/scalable"
    guard_directory "$install_prefix/share/icons/hicolor/scalable/apps"
    guard_directory "$install_prefix/share/licenses"
    guard_directory "$install_prefix/share/licenses/dino"
    guard_directory "$config_directory"

    guard_regular_or_missing "$license"
    guard_regular_or_missing "$binary"
    guard_regular_or_missing "$desktop"
    guard_regular_or_missing "$icon"
    if (( ! standalone )); then
        guard_regular_or_missing "$apps_list"
    fi
    guard_regular_or_missing "$theme_file"

    if [[ -L "$registered" ]]; then
        [[ "$(/usr/bin/readlink -- "$registered")" == "$desktop" ]] \
            || fail "Refusing to replace an unrelated symlink: $registered"
    else
        [[ ! -e "$registered" || -f "$registered" ]] \
            || fail "Expected a regular file or the Dino desktop symlink: $registered"
    fi

    if (( ! standalone )) && [[ -f "$config_home/themes/.caelestia-use/Colors.qml" ]]; then
        "$build_directory/dino" --config-dir "$config_directory" theme follow active --dry-run >/dev/null \
            || fail 'The active palette could not be imported; fix it or use --standalone'
    fi
    "$build_directory/dino" --config-dir "$config_directory" theme reset --dry-run >/dev/null \
        || fail 'Could not initialize Dino theme settings'
}

backup_file() {
    local path="$1"
    [[ -f "$path" ]] || return 0

    local timestamp backup suffix=0
    timestamp="$(/usr/bin/date -u +%Y%m%dT%H%M%SZ)"
    backup="$path.dino-backup.$timestamp.$$"
    while [[ -e "$backup" || -L "$backup" ]]; do
        ((suffix += 1))
        backup="$path.dino-backup.$timestamp.$$.$suffix"
    done
    /usr/bin/cp -p -- "$path" "$backup"
}

new_temporary_file() {
    local directory="$1"
    local prefix="$2"
    local path
    path="$(/usr/bin/mktemp "$directory/.$prefix.XXXXXX")"
    temporary_paths+=("$path")
    REPLY="$path"
}

atomic_install_file() {
    local source="$1"
    local destination="$2"
    local mode="$3"
    local directory temporary current_mode
    directory="${destination%/*}"
    /usr/bin/mkdir -p -- "$directory"
    guard_directory "$directory"

    new_temporary_file "$directory" dino-install
    temporary="$REPLY"
    /usr/bin/install -m "$mode" -- "$source" "$temporary"
    if [[ -f "$destination" ]] && /usr/bin/cmp -s -- "$temporary" "$destination"; then
        current_mode="$(/usr/bin/stat -c '%a' -- "$destination")"
        if [[ "$current_mode" == "$mode" ]]; then
            /usr/bin/rm -f -- "$temporary"
            return 0
        fi
    fi
    backup_file "$destination"
    /usr/bin/mv -T -- "$temporary" "$destination"
}

write_desktop_entry() {
    local destination="$install_prefix/bin/applications/$desktop_id.desktop"
    /usr/bin/mkdir -p -- "${destination%/*}"
    guard_directory "${destination%/*}"
    new_temporary_file "${destination%/*}" dino-desktop
    local temporary="$REPLY"
    /usr/bin/awk -v executable="$install_prefix/bin/dino" -v config="$config_directory" \
        '/^Exec=/ { printf "Exec=\"%s\" --config-dir \"%s\"\n", executable, config; next } { print }' \
        "$project_directory/assets/$desktop_id.desktop" > "$temporary"
    /usr/bin/chmod 644 -- "$temporary"
    if [[ -f "$destination" ]] && /usr/bin/cmp -s -- "$temporary" "$destination" \
        && [[ "$(/usr/bin/stat -c '%a' -- "$destination")" == 644 ]]; then
        /usr/bin/rm -f -- "$temporary"
        return 0
    fi
    backup_file "$destination"
    /usr/bin/mv -T -- "$temporary" "$destination"
}

register_desktop_entry() {
    local desktop="$install_prefix/bin/applications/$desktop_id.desktop"
    local registered="$install_prefix/share/applications/$desktop_id.desktop"
    if [[ -L "$registered" && "$(/usr/bin/readlink -- "$registered")" == "$desktop" ]]; then
        return 0
    fi

    /usr/bin/mkdir -p -- "${registered%/*}"
    guard_directory "${registered%/*}"
    backup_file "$registered"
    new_temporary_file "${registered%/*}" dino-desktop-link
    local temporary="$REPLY"
    /usr/bin/rm -f -- "$temporary"
    /usr/bin/ln -s -- "$desktop" "$temporary"
    /usr/bin/mv -Tf -- "$temporary" "$registered"
}

register_app_list_row() {
    (( standalone )) && return 0

    local icon="$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg"
    local entry="$desktop_id.desktop|$icon|Dino"
    local directory="${apps_list%/*}"
    /usr/bin/mkdir -p -- "$directory"
    guard_directory "$directory"
    new_temporary_file "$directory" dino-apps
    local temporary="$REPLY"
    if [[ -f "$apps_list" ]]; then
        /usr/bin/awk -F '|' -v OFS='|' -v id="$desktop_id.desktop" -v entry="$entry" \
            '$1 == id { if (!seen) print entry; seen = 1; next } { print } END { if (!seen) print entry }' \
            "$apps_list" > "$temporary"
        /usr/bin/chmod --reference="$apps_list" "$temporary"
        if /usr/bin/cmp -s -- "$temporary" "$apps_list"; then
            /usr/bin/rm -f -- "$temporary"
            return 0
        fi
        backup_file "$apps_list"
    else
        /usr/bin/printf '%s\n' "$entry" > "$temporary"
        /usr/bin/chmod 644 -- "$temporary"
    fi
    /usr/bin/mv -T -- "$temporary" "$apps_list"
}

initialize_theme() {
    local theme_file="$config_directory/theme.json"
    [[ -f "$theme_file" ]] && return 0

    /usr/bin/mkdir -p -- "$config_directory"
    guard_directory "$config_directory"
    if (( ! standalone )) && [[ -f "$config_home/themes/.caelestia-use/Colors.qml" ]]; then
        "$install_prefix/bin/dino" --config-dir "$config_directory" theme follow active
    else
        "$install_prefix/bin/dino" --config-dir "$config_directory" theme reset
    fi
}

print_dry_run() {
    /usr/bin/printf 'Install executable: %s/bin/dino\nDesktop entry: %s/bin/applications/%s.desktop\nRegister desktop: %s/share/applications/%s.desktop\nIcon: %s/share/icons/hicolor/scalable/apps/%s.svg\nConfiguration: %s\n' \
        "$install_prefix" "$install_prefix" "$desktop_id" "$install_prefix" "$desktop_id" \
        "$install_prefix" "$desktop_id" "$config_directory"
    /usr/bin/printf 'License: %s/share/licenses/dino/omarchy-dino-MIT.txt\n' "$install_prefix"
    if (( standalone )); then
        /usr/bin/printf '%s\n' 'Standalone: leave apps.list unchanged; initialize a portable theme if needed.'
    else
        /usr/bin/printf 'App list: %s\nTheme: follow active palette when available; otherwise use the built-in palette.\n' \
            "$apps_list"
    fi
}

main() {
    parse_options "$@"
    validate_option_path --prefix "$install_prefix"
    validate_option_path --config-dir "$config_directory"
    validate_option_path --apps-list "$apps_list"
    validate_option_path --build-dir "$build_directory"
    validate_installation

    if (( dry_run )); then
        print_dry_run
        return 0
    fi

    atomic_install_file "$build_directory/dino" "$install_prefix/bin/dino" 755
    atomic_install_file "$project_directory/assets/$desktop_id.svg" \
        "$install_prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg" 644
    atomic_install_file "$project_directory/licenses/omarchy-dino-MIT.txt" \
        "$install_prefix/share/licenses/dino/omarchy-dino-MIT.txt" 644
    write_desktop_entry
    register_desktop_entry
    register_app_list_row
    initialize_theme
    /usr/bin/printf 'Installed Dino to %s/bin/dino.\n' "$install_prefix"
}

main "$@"
