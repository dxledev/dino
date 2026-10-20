#!/usr/bin/env bash
set -euo pipefail

if (( $# != 2 )); then
    /usr/bin/printf 'Usage: %s PROJECT_ROOT BUILD_DIR\n' "${0##*/}" >&2
    exit 2
fi

project_root="$(cd -- "$1" && /usr/bin/pwd -P)"
build_directory="$(cd -- "$2" && /usr/bin/pwd -P)"
installer="$project_root/scripts/install-desktop.sh"
binary="$build_directory/dino"
desktop_id="io.github.dino_runner"
temporary_directory="$(/usr/bin/mktemp -d "${TMPDIR:-/tmp}/dino-install-test.XXXXXX")"

cleanup() {
    /usr/bin/rm -rf -- "$temporary_directory"
}

trap cleanup EXIT

fail() {
    /usr/bin/printf 'install test: %s\n' "$*" >&2
    exit 1
}

check() {
    "$@" || fail "check failed: $*"
}

[[ -x "$binary" ]] || fail "Missing executable: $binary"

prefix="$temporary_directory/install prefix"
config_directory="$temporary_directory/config directory/dino"
apps_list="$temporary_directory/config directory/apps.list"
xdg_config_home="$temporary_directory/xdg config"
active_palette="$xdg_config_home/themes/.caelestia-use/Colors.qml"
/usr/bin/mkdir -p -- "${active_palette%/*}" "$prefix/bin"
/usr/bin/mkdir -p -- "${apps_list%/*}"
/usr/bin/cat > "$active_palette" <<'QML'
QtObject {
    property color background: "#ff101010"
    property color foreground: "#ffeeeeee"
    property color primary: "#ff50a0f0"
}
QML

printf 'first.desktop|first.svg|First\n%s.desktop|old.svg|Old Dino\nsecond.desktop|second.svg|Second\n%s.desktop|duplicate.svg|Dino\n' \
    "$desktop_id" "$desktop_id" > "$apps_list"
original_apps_list="$temporary_directory/apps-list-original"
/usr/bin/cp -- "$apps_list" "$original_apps_list"
/usr/bin/printf 'old Dino binary\n' > "$prefix/bin/dino"

XDG_CONFIG_HOME="$xdg_config_home" "$installer" --dry-run \
    --prefix "$prefix" --config-dir "$config_directory" --apps-list "$apps_list" \
    --build-dir "$build_directory" > "$temporary_directory/dry-run.txt"
check /usr/bin/grep -Fq "Install executable: $prefix/bin/dino" "$temporary_directory/dry-run.txt"
check /usr/bin/test ! -e "$config_directory/theme.json"
check /usr/bin/cmp -s "$apps_list" "$original_apps_list"

XDG_CONFIG_HOME="$xdg_config_home" "$installer" \
    --prefix "$prefix" --config-dir "$config_directory" --apps-list "$apps_list" \
    --build-dir "$build_directory" > "$temporary_directory/install-output.txt"

check /usr/bin/cmp -s "$binary" "$prefix/bin/dino"
check /usr/bin/cmp -s "$project_root/licenses/omarchy-dino-MIT.txt" \
    "$prefix/share/licenses/dino/omarchy-dino-MIT.txt"
check /usr/bin/test "$(/usr/bin/stat -c '%a' "$prefix/bin/dino")" = 755
check /usr/bin/test -f "$prefix/bin/applications/$desktop_id.desktop"
check /usr/bin/test "$(/usr/bin/readlink -- "$prefix/share/applications/$desktop_id.desktop")" = \
    "$prefix/bin/applications/$desktop_id.desktop"
check /usr/bin/grep -Fxq "Exec=\"$prefix/bin/dino\" --config-dir \"$config_directory\"" \
    "$prefix/bin/applications/$desktop_id.desktop"
check /usr/bin/test -f "$prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg"
check /usr/bin/grep -Fq 'fill="currentColor"' \
    "$prefix/share/icons/hicolor/scalable/apps/$desktop_id.svg"

expected_apps_list="$temporary_directory/apps-list-expected"
printf 'first.desktop|first.svg|First\n%s.desktop|%s/share/icons/hicolor/scalable/apps/%s.svg|Dino\nsecond.desktop|second.svg|Second\n' \
    "$desktop_id" "$prefix" "$desktop_id" > "$expected_apps_list"
check /usr/bin/cmp -s "$apps_list" "$expected_apps_list"
check /usr/bin/grep -Fq "$active_palette" "$config_directory/theme.json"

binary_backup="$(/usr/bin/find "$prefix/bin" -maxdepth 1 -type f -name 'dino.dino-backup.*' -print -quit)"
apps_backup="$(/usr/bin/find "${apps_list%/*}" -maxdepth 1 -type f -name 'apps.list.dino-backup.*' -print -quit)"
check /usr/bin/test -n "$binary_backup"
check /usr/bin/test -n "$apps_backup"
check /usr/bin/grep -Fq 'old Dino binary' "$binary_backup"
check /usr/bin/cmp -s "$apps_backup" "$original_apps_list"

standalone_prefix="$temporary_directory/standalone"
standalone_config="$temporary_directory/standalone config/dino"
before_standalone="$temporary_directory/apps-list-before-standalone"
/usr/bin/cp -- "$apps_list" "$before_standalone"
XDG_CONFIG_HOME="$xdg_config_home" "$installer" --standalone \
    --prefix "$standalone_prefix" --config-dir "$standalone_config" --apps-list "$apps_list" \
    --build-dir "$build_directory" > /dev/null
check /usr/bin/cmp -s "$apps_list" "$before_standalone"
standalone_theme="$("$standalone_prefix/bin/dino" --config-dir "$standalone_config" theme show)"
[[ "$standalone_theme" != *"$active_palette"* ]] || fail 'Standalone mode followed the active palette'

fallback_prefix="$temporary_directory/no-active-prefix"
fallback_config="$temporary_directory/no-active-config/dino"
fallback_apps="$temporary_directory/no-active-config/apps.list"
empty_xdg="$temporary_directory/no-active-xdg"
XDG_CONFIG_HOME="$empty_xdg" "$installer" \
    --prefix "$fallback_prefix" --config-dir "$fallback_config" --apps-list "$fallback_apps" \
    --build-dir "$build_directory" > /dev/null
fallback_theme="$("$fallback_prefix/bin/dino" --config-dir "$fallback_config" theme show)"
[[ "$fallback_theme" != *'"source"'* ]] || fail 'Missing active palette did not use the built-in palette'

guard_prefix="$temporary_directory/symlink-guard"
/usr/bin/mkdir -p -- "$guard_prefix/bin"
/usr/bin/ln -s -- "$binary" "$guard_prefix/bin/dino"
if XDG_CONFIG_HOME="$empty_xdg" "$installer" \
    --prefix "$guard_prefix" --config-dir "$temporary_directory/guard-config/dino" \
    --apps-list "$temporary_directory/guard-config/apps.list" --build-dir "$build_directory" \
    > /dev/null 2>&1; then
    fail 'Installer replaced a symlinked executable'
fi
check /usr/bin/test -L "$guard_prefix/bin/dino"
check /usr/bin/test "$(/usr/bin/readlink -- "$guard_prefix/bin/dino")" = "$binary"

/usr/bin/printf '%s\n' 'Install script checks passed.'
