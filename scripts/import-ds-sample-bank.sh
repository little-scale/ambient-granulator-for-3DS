#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
default_source="/Users/a1106632/Documents/nds-locked-granulator /assets/sample_bank.bin"
source_bank="${1:-$default_source}"
destination="$project_dir/romfs/sample_bank.bin"

if [[ ! -f "$source_bank" ]]; then
    echo "Sample bank not found: $source_bank" >&2
    exit 1
fi

mkdir -p "$project_dir/romfs"
perl "$project_dir/tools/compact-sample-bank.pl" \
    "$source_bank" "$destination"
shasum -a 256 "$destination"
