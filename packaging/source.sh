#!/usr/bin/env bash
# Complete corresponding source, including both reviewed submodules; never Git metadata.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:?usage: packaging/source.sh OUTPUT_DIRECTORY}"
VERSION="$(sed -n 's/^project(UmbrielConnect VERSION \([^ ]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
python3 "$ROOT/scripts/source_integrity.py"
[[ -z "$(git -C "$ROOT" status --porcelain)" ]] || { echo 'Commit the release sources before exporting.' >&2; exit 1; }
mkdir -p "$OUT"
WORK="$(mktemp -d)";trap 'rm -rf "$WORK"' EXIT
SOURCE_DATE_EPOCH="$(git -C "$ROOT" show -s --format=%ct HEAD)"
mkdir "$WORK/umbriel-connect-$VERSION"
git -C "$ROOT" archive HEAD | tar -x -C "$WORK/umbriel-connect-$VERSION"
for module in kdeconnect-kde kdeconnect-android; do
 mkdir -p "$WORK/umbriel-connect-$VERSION/vendor/$module"
 git -C "$ROOT/vendor/$module" archive HEAD | tar -x -C "$WORK/umbriel-connect-$VERSION/vendor/$module"
done
python3 "$WORK/umbriel-connect-$VERSION/scripts/source_integrity.py"
tar --sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0 --numeric-owner -C "$WORK" -cJf "$OUT/umbriel-connect-$VERSION-source.tar.xz" "umbriel-connect-$VERSION"
