#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
version="${APP_VERSION:-0.2.0}"
git_hash="UNKNOWN"
dirty=""

if git -C "$project_dir" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git_hash="$(git -C "$project_dir" rev-parse --short=7 HEAD \
        | tr '[:lower:]' '[:upper:]')"
    if [[ -n "$(git -C "$project_dir" status --porcelain \
            --untracked-files=normal)" ]]; then
        dirty="+"
    fi
fi

mkdir -p "$project_dir/build/3ds"
header="$project_dir/build/3ds/build_info.h"
temporary="$header.tmp"
printf '%s\n' \
    '#ifndef GRANULATOR_BUILD_INFO_H' \
    '#define GRANULATOR_BUILD_INFO_H' \
    "#define APP_VERSION \"$version\"" \
    "#define APP_GIT_HASH \"$git_hash\"" \
    "#define APP_GIT_DIRTY \"$dirty\"" \
    '#define APP_BUILD_ID "V" APP_VERSION " " APP_GIT_HASH APP_GIT_DIRTY' \
    '#endif' \
    > "$temporary"

if [[ ! -f "$header" ]] || ! cmp -s "$temporary" "$header"; then
    mv "$temporary" "$header"
else
    rm -f "$temporary"
fi
