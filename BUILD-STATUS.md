# Build status — can we build from this tree?

**Answer: Yes.** From this repository the VirtualBox **core builds successfully** —
including the VMM hypervisor engine, the IPRT runtime, the support library, the
disassembler, the Main/COM API layer, and the full XPCOM stack. `configure`
succeeds, the vendored `kmk` runs, and the top-level **Build Programs** and
**Libraries** passes complete cleanly.

## Environment
- Amazon Linux 2023, x86_64; gcc 11.5.0, clang 15.0.7, GNU make, yasm 1.3.0, nasm, bison, cmake, perl
- Vendored kBuild `kmk` r3513 (prebuilt binary imported into `kBuild/`)
- Host libs present: libxml2 (+dev), openssl, curl, zlib, libpng, libjpeg, liblzma
- Network restricted (no `dnf`/`pip` package installs)

## Results — what builds ✅

Top-level `pass_bldprogs` and `pass_libraries` both complete (exit 0).
**3,929 objects compiled; 43 libraries produced**, including:

| Library | What it is |
|---------|-----------|
| **`VMMStatic.a`** | **The Virtual Machine Monitor — the core hypervisor** |
| **`RuntimeR3.a` / `RuntimeR0.a`** | IPRT runtime (ring-3 / ring-0) |
| **`SUPR3.a` / `SUPR3Static.a`** | Support library (host↔VMM) |
| **`DisasmR3.a` / `DisasmR0.a`** | Instruction disassembler |
| **`VBoxAPIWrap.a` / `VBoxCOM.a`** | Main API layer (XIDL→code via xsltproc) |
| `VBox-xpcom-*.a` (18 libs) | Full XPCOM component/IPC/typelib stack |
| `VBoxRTImp.so` / `VBoxXPCOMImp.so` | Linked **shared** libraries |
| `VBox-SoftFloat*.a`, `VBox-lwip*.a`, `VBox-libslirp.a`, `VBox-libogg/vorbis/lzf.a` | In-tree third-party libs built from source |
| `PcBiosBin.a`, `VgaBiosBin.a`, `iPxeBiosBin.a` | Guest BIOS / iPXE ROM images |
| `StorageLib.a`, `USBLib.a`, `Debugger.a`, guest-additions R3 libs | Storage, USB, debugger, additions |

Also builds and runs the build tools: `VBoxTpG`, `VBoxCPP`, `xpidl`, `bin2c`,
`biossums`, `genalias`, `filesplitter`, `VBoxCmp`, `MakeAlternativeSource`.

## Fixes/workarounds needed to get here

These are **host/environment** adjustments — none required changing the imported
source:

1. **`xsltproc` missing** → built libxslt from source (cloned `GNOME/libxslt`,
   compiled with cmake against the system libxml2) and put `xsltproc` on `PATH`.
   Needed for XIDL→API code generation and man-page generation.
2. **Empty SCM revision** → the git import has no SVN revision, so
   `IPRT_BLDCFG_SCM_REV` expanded to nothing and `buildconfig.cpp` failed. Pass
   `VBOX_SVN_REV=<number>` (any integer) to fix.
3. **PAM headers missing** (`security/pam_appl.h`) → pass `IPRT_WITHOUT_PAM=1`.
4. Warnings-as-policy relaxed with `VBOX_WITH_NO_GCC_WARNING_POLICY=1`.

### Reproduce

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

## Not yet built (needs more host deps, not source changes)

- Final **executables/installer** (`VBoxManage`, `VBoxHeadless`, `VBoxSVC`): the
  program-linking pass pulls in more optional deps; the GUI (`VirtualBox`) needs
  **Qt6** (disabled here).
- Linux **kernel modules** (`vboxdrv`, etc.): need kernel headers (`--disable-kmods`).
- Full product package: needs `makeself`, and various `-dev` libraries
  (device-mapper, libcap, PulseAudio, Vulkan, …).

## Conclusion

The imported tree is a **genuinely buildable source tree**: the core engine and
libraries compile and link from source on this host. Remaining gaps are host
tooling/dependencies (Qt6, PAM/devmapper/etc. dev packages, kernel headers,
makeself) — not deficiencies in the imported code — plus optional binary assets
noted earlier.
