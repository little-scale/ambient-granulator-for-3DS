#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
artifact="$project_dir/3ds_granulator.3dsx"

if [[ ! -s "$artifact" ]]; then
    "$project_dir/scripts/build.sh"
fi

if [[ -n "${AZAHAR_BIN:-}" && -x "$AZAHAR_BIN" ]]; then
    emulator="$AZAHAR_BIN"
elif [[ -x /Applications/Azahar.app/Contents/MacOS/azahar ]]; then
    emulator=/Applications/Azahar.app/Contents/MacOS/azahar
elif [[ -x /Applications/Azahar.app/Contents/MacOS/Azahar ]]; then
    emulator=/Applications/Azahar.app/Contents/MacOS/Azahar
elif command -v azahar >/dev/null 2>&1; then
    emulator="$(command -v azahar)"
elif [[ -x /Applications/Citra.app/Contents/MacOS/citra-qt ]]; then
    emulator=/Applications/Citra.app/Contents/MacOS/citra-qt
elif command -v citra-qt >/dev/null 2>&1; then
    emulator="$(command -v citra-qt)"
else
    echo "Azahar was not found. Install it or set AZAHAR_BIN." >&2
    exit 1
fi

if [[ "$emulator" == *Azahar.app* || "$emulator" == *azahar* ]]; then
    azahar_data="${AZAHAR_USER_DIR:-$HOME/Library/Application Support/Azahar}"
    dspfirm="$azahar_data/sdmc/3ds/dspfirm.cdc"
    if [[ ! -s "$dspfirm" ]]; then
        printf '%s\n' \
            'Warning: Azahar virtual SD has no /3ds/dspfirm.cdc.' \
            'The visual test will run, but libctru NDSP will report D880A7FA.' \
            'Dump DSP firmware from your own console, then run:' \
            '  ./scripts/install-emulator-dsp.sh /path/to/SD/3ds/dspfirm.cdc' \
            >&2
    fi
fi

exec "$emulator" "$artifact"
