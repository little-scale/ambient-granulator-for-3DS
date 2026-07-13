#!/usr/bin/env bash
set -euo pipefail

readonly image="devkitpro/devkitarm:20260610"

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker Desktop is required for the pinned 3DS toolchain." >&2
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "Start Docker Desktop, then run this script again." >&2
    exit 1
fi

docker pull "$image"
docker image inspect "$image" --format 'Toolchain: {{index .RepoDigests 0}}'

docker run --rm "$image" bash -lc '
    dkp-pacman -Q | grep -E "^(devkitARM|libctru|citro2d|citro3d|3ds-cmake) "
'

echo "Native Nintendo 3DS toolchain is ready."
