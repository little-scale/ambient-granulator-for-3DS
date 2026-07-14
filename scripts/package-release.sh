#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
version="$(tr -d '[:space:]' < "$project_dir/VERSION")"
release_dir="$project_dir/dist/release/v$version"
binary_name="ambient-granulator-for-3DS-v$version.3dsx"
zip_name="ambient-granulator-for-3DS-v$version-sd.zip"

if [[ -n "$(git -C "$project_dir" status --porcelain)" ]]; then
    echo "Release builds require a clean committed worktree." >&2
    exit 1
fi

"$project_dir/scripts/verify-build.sh"
"$project_dir/scripts/package-sd.sh"

mkdir -p "$release_dir"
staging="$(mktemp -d)"
trap 'rm -rf "$staging"' EXIT

mkdir -p "$staging/3ds/3ds-granulator"
cp "$project_dir/3ds_granulator.3dsx" \
   "$staging/3ds/3ds-granulator/3ds_granulator.3dsx"
cp "$project_dir/3ds_granulator.smdh" \
   "$staging/3ds/3ds-granulator/3ds_granulator.smdh"
cp "$project_dir/3ds_granulator.3dsx" "$release_dir/$binary_name"

(
    cd "$staging"
    zip -qr "$staging/$zip_name" 3ds
)
mv "$staging/$zip_name" "$release_dir/$zip_name"

(
    cd "$release_dir"
    shasum -a 256 "$binary_name" "$zip_name"
) > "$staging/SHA256SUMS.txt"
mv "$staging/SHA256SUMS.txt" "$release_dir/SHA256SUMS.txt"

echo "Release assets: $release_dir"
