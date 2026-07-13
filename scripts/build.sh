#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
readonly image="devkitpro/devkitarm:20260610"

"$project_dir/scripts/generate-build-info.sh"

if ! docker image inspect "$image" >/dev/null 2>&1; then
    "$project_dir/scripts/setup.sh"
fi

docker run --rm \
    --user "$(id -u):$(id -g)" \
    --volume "$project_dir:/work" \
    --workdir /work \
    "$image" \
    make "$@"
