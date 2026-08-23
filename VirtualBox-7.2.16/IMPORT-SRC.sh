#!/usr/bin/env bash
set -euo pipefail

# Imports the upstream VirtualBox source tree without flattening or rewriting it.
# Run from the root of meark/Virtual-Machine.
VERSION="7.2.16"
UPSTREAM="https://github.com/VirtualBox/virtualbox.git"
DEST="VirtualBox-${VERSION}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

git clone --filter=blob:none --no-checkout "$UPSTREAM" "$TMP/virtualbox"
cd "$TMP/virtualbox"
if ! git show-ref --verify --quiet "refs/tags/v${VERSION}"; then
  echo "Upstream tag v${VERSION} was not found; inspect available tags before importing." >&2
  exit 2
fi
git checkout --detach "v${VERSION}"
cd - >/dev/null

rm -rf "${DEST}/src"
mkdir -p "${DEST}"
cp -a "$TMP/virtualbox/src" "${DEST}/src"

cat > "${DEST}/SOURCE-VERSION.md" <<EOF
# VirtualBox source import

This directory contains the upstream VirtualBox **src** tree for version ${VERSION}.

Upstream: ${UPSTREAM}
Requested import: src only

The import is intentionally kept under `VirtualBox-${VERSION}/` and does not replace
other repository material. Preserve the upstream licensing and attribution files
when distributing or building this source.
EOF

echo "Imported ${DEST}/src"
