# VirtualBox `src/` import — text-only (no Git LFS required)

## What was imported

The upstream VirtualBox `src/` tree was imported from the GitHub mirror
(`github.com/VirtualBox/virtualbox`), branch **`VBox-7.2`** (currently version
**7.2.17** — see note below), **excluding all binary files** so that **no file
requires Git LFS**.

| Metric | Count |
|--------|------:|
| Upstream `src/` files (total)     | 63,701 |
| **Imported (text files)**         | **57,449** |
| Excluded (binary files)           | 6,252 |
| Largest imported file             | ~13.6 MB |
| Files ≥ 50 MB (GitHub warns)      | 0 |
| Files ≥ 100 MB (GitHub rejects)   | 0 |

Binary vs. text classification was done with Git's own detection
(`git diff --numstat`, where binary files report `-`), not by extension guessing.
The staged set was re-verified to contain **0 binary files**.

### Imported components

| Component        | Text files imported |
|------------------|--------------------:|
| `src/VBox/`      | 43,533 |
| `src/libs/`      | 13,865 |
| `src/bldprogs/`  | 36 |
| `src/apps/`      | 14 |
| `src/Makefile.kmk` | 1 |

## What was excluded (6,252 binary files → would need LFS)

These are the assets that are impractical to store in plain Git and are exactly
what LFS exists for. Excluded by component: **`src/VBox/` 4,726**, **`src/libs/` 1,526**.

Predominant types excluded:

| Type | Count | | Type | Count |
|------|------:|-|------|------:|
| `.png`  | 4,405 | | `.ico` | 18 |
| `.gz`   | 419   | | `.pcap`| 17 |
| `.der`  | 90    | | `.ova` | 12 |
| `.p12`  | 35    | | `.crt` | 12 |
| `.taf`  | 18    | | `.bmp` | 12 |

…plus firmware blobs (`.fd`), fuzzing corpora (extensionless hash-named files),
archives (`.zip`), keys/certs, `.icns`, `.jpg`, `.pdf`, and similar.

The complete list of excluded files is in **`EXCLUDED-binary-files.txt`**.

## Note on version

The importer script `VirtualBox-7.2.16/IMPORT-SRC.sh` pins tag `v7.2.16`, but the
GitHub mirror publishes **no tags** — only branches. The `VBox-7.2` branch has
advanced to **7.2.17** (`VBOX_VERSION_BUILD = 17`). The exact 7.2.16 tree is not
retrievable from this mirror, so this import is from `VBox-7.2` @ 7.2.17. File
layout is stable across patch releases.

## Licensing

The imported tree includes Oracle VirtualBox source (GPL/CDDL) and bundled
third-party libraries under their own licenses. Upstream license and attribution
files were imported wherever they are text; preserve them when redistributing.
