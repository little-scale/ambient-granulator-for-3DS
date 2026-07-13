#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /path/to/your/3DS-SD/3ds/dspfirm.cdc" >&2
    exit 2
fi

source_firmware="$1"
if [[ ! -f "$source_firmware" || ! -s "$source_firmware" ]]; then
    echo "DSP firmware file is missing or empty: $source_firmware" >&2
    exit 1
fi

azahar_data="${AZAHAR_USER_DIR:-$HOME/Library/Application Support/Azahar}"
destination_dir="$azahar_data/sdmc/3ds"
destination="$destination_dir/dspfirm.cdc"

mkdir -p "$destination_dir"
cp "$source_firmware" "$destination"

echo "Installed your console's DSP firmware into Azahar:"
echo "$destination"
