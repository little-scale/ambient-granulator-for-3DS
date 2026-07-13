#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
image="devkitpro/devkitarm:20260610"

"$project_dir/scripts/test.sh"
"$project_dir/scripts/build.sh"

test -s "$project_dir/3ds_granulator.3dsx"
test -s "$project_dir/3ds_granulator.elf"
test -s "$project_dir/3ds_granulator.smdh"

magic="$(od -An -t x1 -N 4 "$project_dir/3ds_granulator.3dsx" \
    | tr -d ' \n')"
if [[ "$magic" != "33445358" ]]; then
    echo "Unexpected .3dsx magic: $magic" >&2
    exit 1
fi

docker run --rm \
    --volume "$project_dir:/work:ro" \
    --workdir /work \
    "$image" \
    /opt/devkitpro/devkitARM/bin/arm-none-eabi-readelf -h 3ds_granulator.elf

shasum -a 256 "$project_dir/3ds_granulator.3dsx"
echo "Native .3dsx build verification passed."
