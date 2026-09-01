# Verification Report — mearvk/Virtual-Machine

This document records how this repository was verified against upstream Oracle
VirtualBox and confirmed to be a genuinely buildable source tree. It consolidates
the findings that are also captured in `BUILD-STATUS.md` and the various import
reports.

---

## 1. Scope & reference

- **Repository:** `github.com/mearvk/Virtual-Machine`
- **Upstream reference:** `github.com/VirtualBox/virtualbox`, branch **`VBox-7.2`**
- **Version note:** The repo names `7.2.16`, but the upstream mirror publishes
  **no tags** — only branches. The `VBox-7.2` branch is at **7.2.17**
  (`VBOX_VERSION_BUILD = 17`), so the exact `v7.2.16` tree is not retrievable.
  File layout is stable across patch releases, so all comparisons use
  `VBox-7.2` @ 7.2.17.

---

## 2. Source & documentation audit

Initial state contained only the public headers plus a couple of peripheral tool
directories and an unrun importer script. The full engine, libraries, build
system, and metadata were absent.

The complete upstream tree was then imported (see §4), reaching full parity.

| Area | Upstream | On `main` | Missing |
|------|---------:|----------:|--------:|
| `include/` | 618 | 618 | 0 |
| `src/` | 63,701 | 63,701 | 0 |
| root build files + `tools/` + `doc/` + `debian/` + `.github/` | — | complete | 0 |
| `kBuild/` (vendored) | submodule | vendored in full | 0 |

**Parity result:** 0 upstream files missing (only intentional drop: `.gitmodules`,
because kBuild is vendored as real files rather than a submodule).

---

## 3. No Git LFS required

Every file was classified text-vs-binary with Git's own detector
(`git diff --numstat`). Nothing in the entire tree reaches GitHub's thresholds:

- Largest file overall: **~13.6 MB** (a text XML)
- Files ≥ 50 MB (GitHub warning): **0**
- Files ≥ 100 MB (GitHub hard reject): **0**

So the whole tree — including binary assets (prebuilt `kmk`, EFI firmware `.fd`,
artwork, test data) — is stored in **plain Git, no LFS**.

---

## 4. Import history (all merged to `main`)

| PR | Content | Files |
|----|---------|------:|
| #1 | `src/` tree (text) | 57,449 |
| #2 | Build infra: `configure`, `*.kmk`, `kBuild` source, `tools/`, `doc/`, licenses | ~4,477 |
| #3 | Remaining binaries: prebuilt `kmk`, EFI firmware, artwork, test data, certs | 6,901 |
| #4 | 131 files unblocked from `.gitignore` (force-added) | 131 |
| #5 | Initial `BUILD-STATUS.md` | 1 |
| #6 | Updated `BUILD-STATUS.md` (core build results) | 1 |

---

## 5. Build verification

**Environment:** Amazon Linux 2023, x86_64; gcc 11.5.0, clang 15.0.7, GNU make,
yasm 1.3.0, nasm, bison, cmake, perl. Vendored kBuild `kmk` r3513. Host libs
present: libxml2 (+dev), openssl, curl, zlib, libpng, libjpeg, liblzma. Network
restricted (no `dnf`/`pip` installs).

### 5.1 configure — SUCCESS

Generated `AutoConfig.kmk` and `env.sh`. Detected core deps and opted to build
liblzf/libogg/libvorbis from the in-tree source.

### 5.2 kmk — RUNS

The vendored prebuilt `kmk` (kBuild r3513, GNU Make 4.2.1 based) runs directly;
no bootstrap needed.

### 5.3 Core build — SUCCESS

Top-level `pass_bldprogs` and `pass_libraries` both complete cleanly (exit 0):
**3,929 objects compiled, 43 libraries produced**, including:

| Library | What it is |
|---------|-----------|
| **`VMMStatic.a`** | **The Virtual Machine Monitor — the core hypervisor** |
| `RuntimeR3.a` / `RuntimeR0.a` | IPRT runtime (ring-3 / ring-0) |
| `SUPR3.a` / `SUPR3Static.a` | Support library (host ↔ VMM) |
| `DisasmR3.a` / `DisasmR0.a` | Instruction disassembler |
| `VBoxAPIWrap.a` / `VBoxCOM.a` | Main API layer (XIDL → code via xsltproc) |
| `VBox-xpcom-*.a` (18 libs) | Full XPCOM component/IPC/typelib stack |
| `VBoxRTImp.so` / `VBoxXPCOMImp.so` | Linked **shared** libraries |
| `VBox-SoftFloat*.a`, `VBox-lwip*.a`, `VBox-libslirp.a`, `VBox-libogg/vorbis/lzf.a` | In-tree third-party libs from source |
| `PcBiosBin.a`, `VgaBiosBin.a`, `iPxeBiosBin.a` | Guest BIOS / iPXE ROM images |
| `StorageLib.a`, `USBLib.a`, `Debugger.a`, guest-additions R3 libs | Storage, USB, debugger, additions |

Build tools built and runnable: `VBoxTpG`, `VBoxCPP`, `xpidl`, `bin2c`,
`biossums`, `genalias`, `filesplitter`, `VBoxCmp`, `MakeAlternativeSource`.

### 5.4 Host workarounds (no source changes)

1. **`xsltproc` missing** → built libxslt from source (`GNOME/libxslt`, cmake
   against system libxml2) and placed `xsltproc` on `PATH`. Needed for XIDL→API
   codegen and man-page generation.
2. **Empty SCM revision** → git import has no SVN revision, so
   `IPRT_BLDCFG_SCM_REV` was empty and broke `buildconfig.cpp`. Pass
   `VBOX_SVN_REV=<integer>`.
3. **PAM headers missing** (`security/pam_appl.h`) → pass `IPRT_WITHOUT_PAM=1`.
4. Warning policy relaxed with `VBOX_WITH_NO_GCC_WARNING_POLICY=1`.

### 5.5 Reproduce

```sh
./configure --disable-hardening --disable-qt --disable-sdl --disable-sdl-ttf \
  --disable-pulse --disable-alsa --disable-dbus --disable-opengl --disable-dxvk \
  --disable-vmmraw --disable-python --disable-java --disable-docs --disable-kmods \
  --disable-libvpx --disable-libtpms --disable-extpack --nofatal
source ./env.sh
# xsltproc must be on PATH (build from GNOME/libxslt if absent)
kmk VBOX_WITH_NO_GCC_WARNING_POLICY=1 VBOX_SVN_REV=162700 IPRT_WITHOUT_PAM=1 \
    pass_bldprogs pass_libraries
```

---

## 6. Not yet built (host dependencies, not source issues)

- Final executables / installer (`VBoxManage`, `VBoxHeadless`, `VBoxSVC`): the
  program-linking pass pulls in more optional deps.
- GUI (`VirtualBox`): needs **Qt6** (disabled here).
- Linux kernel modules (`vboxdrv`, …): need kernel headers (`--disable-kmods`).
- Full product package: needs `makeself` and various `-dev` libraries
  (device-mapper, libcap, PulseAudio, Vulkan, …).

---

## 7. Conclusion

- **Completeness:** full parity with upstream `VBox-7.2` (7.2.17); 0 files missing.
- **Storage:** entire tree in plain Git; no LFS needed (largest file ~13.6 MB).
- **Buildability:** confirmed — `configure` succeeds, `kmk` runs, and the core
  (VMM hypervisor, IPRT, SUP, disassembler, Main API, XPCOM) compiles and links
  from source. Remaining gaps are host toolchain/dependencies, not the imported
  code.
