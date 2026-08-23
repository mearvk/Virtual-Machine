# VirtualBox 7.2.16 source import

The upstream VirtualBox repository exposes a substantial `src/` tree containing `Makefile.kmk`, `VBox`, `apps`, `bldprogs`, and `libs`. This repository now contains a reproducible importer for that source tree.

## Import

Run `VirtualBox-7.2.16/IMPORT-SRC.sh` from the repository root. It clones the upstream repository, checks out the exact `v7.2.16` tag, and copies only `src/` into `VirtualBox-7.2.16/src/`.

The importer intentionally fails if the requested upstream tag is unavailable rather than silently substituting another revision.

## Important

The GitHub Contents API is appropriate for individual text files but is not a reliable bulk-transfer mechanism for the entire VirtualBox source tree. Therefore this commit adds the exact, reproducible bulk-import operation rather than pretending that a partial API upload is the complete source tree.

After running the importer locally or in CI, verify the resulting tree and commit it using normal Git operations. Keep upstream licensing and attribution intact.

## Upstream source layout verified

The upstream `src/` directory currently contains at least:

- `Makefile.kmk`
- `VBox/`
- `apps/`
- `bldprogs/`
- `libs/`

This document does not claim that the destination repository is build-complete until the imported tree and the required top-level VirtualBox build dependencies are present.
