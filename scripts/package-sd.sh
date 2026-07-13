#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
destination="$project_dir/dist/3ds/3ds-granulator"

"$project_dir/scripts/build.sh"
mkdir -p "$destination"
cp "$project_dir/3ds_granulator.3dsx" "$destination/3ds_granulator.3dsx"
cp "$project_dir/3ds_granulator.smdh" "$destination/3ds_granulator.smdh"

echo "SD-card package: $destination"
