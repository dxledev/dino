#!/usr/bin/env bash
set -euo pipefail
binary="$1"
temporary="$(mktemp -d)"
trap 'rm -rf -- "$temporary"' EXIT
export DINO_CONFIG_DIR="$temporary/config"
"$binary" --help >/dev/null
"$binary" theme reset --dry-run >/dev/null
[[ ! -e "$DINO_CONFIG_DIR" ]]
"$binary" theme reset
"$binary" theme set 'background=#e00e1e36' 'accent=#c4a7e7'
"$binary" theme show > "$temporary/theme.json"
rg -q '#e00e1e36' "$temporary/theme.json"
cp "$DINO_CONFIG_DIR/theme.json" "$temporary/before.json"
if "$binary" theme set 'accent=bad' 2>/dev/null; then exit 1; fi
cmp "$DINO_CONFIG_DIR/theme.json" "$temporary/before.json"
"$binary" theme set 'background=Qt.rgba(14 / 255, 30 / 255, 54 / 255, 0.88)'
"$binary" theme show | rg -q '#e00e1e36'
"$binary" theme follow "$DINO_CONFIG_DIR/theme.json" 2>/dev/null && exit 1
"$binary" --config-dir "$temporary/missing" theme set 'accent=#ffffff' --dry-run >/dev/null
[[ ! -e "$temporary/missing" ]]
if "$binary" --not-an-option 2>/dev/null; then exit 1; fi
mkfifo "$temporary/palette.qml"
if timeout 2s "$binary" theme import "$temporary/palette.qml" --dry-run 2>/dev/null; then exit 1; else
    [[ $? != 124 ]]
fi
printf 'cli: ok\n'
