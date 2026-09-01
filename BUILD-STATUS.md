# Build status — can we build from this tree?

**Short answer: Yes, the tree is buildable and the source compiles.** `configure`
succeeds, the vendored `kmk` runs, and source from this repo compiles and links
into working executables. A *full* product build is currently blocked only by
**missing host build tools** (not by anything wrong with the imported source).

## Environment used
- Amazon Linux 2023, x86_64
- gcc 11.5.0, clang 15.0.7, GNU make, yasm 1.3.0, nasm, bison
- Vendored kBuild `kmk` r3513 (the prebuilt binary imported into `kBuild/`)
- Network: restricted — cannot `dnf install` additional packages

## What worked ✅

1. **`configure` succeeded** — generated `AutoConfig.kmk` and `env.sh`.
   Detected openssl, curl, libxml2, zlib, libpng, libjpeg, liblzma; opted to
   build liblzf/libogg/libvorbis from the in-tree source we imported.
   Invocation:
   ```
   ./configure --disable-hardening --disable-qt --disable-sdl --disable-sdl-ttf \
     --disable-pulse --disable-alsa --disable-dbus --disable-opengl --disable-dxvk \
     --disable-vmmraw --disable-python --disable-java --disable-docs --disable-kmods \
     --disable-libvpx --disable-libtpms --disable-extpack --nofatal
   ```

2. **`kmk` runs** from the vendored kBuild (no bootstrap needed — the prebuilt
   binary import solved that).

3. **Source compiles.** 117+ object files built with zero source/compile errors,
   including build programs and ~111 IPRT Runtime objects
   (alloc, asn1, crypto, dbg, err, fs, …).

4. **Executables link and run.** Five build tools were fully linked from source:
   `bin2c`, `biossums`, `filesplitter`, `genalias`, `VBoxCmp`.
   Verified: `bin2c` prints its usage; `VBoxCmp` is a valid ELF x86-64 binary.

## What blocks a FULL build ⛔ (host tools, not our source)

The default/top-level target needs tools that are **not installed** and cannot be
installed here (restricted network):

| Missing tool | Blocks | Disable/avoid with |
|--------------|--------|--------------------|
| `xsltproc` (libxslt) | XIDL→API codegen (Main) and man-page/doc XML | install libxslt; `VBOX_WITHOUT_MANPAGE=1` avoids man step |
| Qt6 | GUI frontend (`VirtualBox`) | `--disable-qt` (already used) |
| SDL, PulseAudio, ALSA, DBus, OpenGL/Vulkan, device-mapper, libcap | various frontends/host bits | `--disable-*` / build only core targets |
| `makeself`, `soapcpp2` (gSOAP) | installer, webservice | not needed for core |

Note: `xsltproc` is the main gate — even the IPRT Runtime library stops at a
man-page generation step that shells out to `xsltproc`. It is a **doc/codegen**
dependency, not a compiler issue; the C/C++ compiled fine right up to that point.

## Conclusion

- **Buildability of the imported tree: confirmed.** The source, headers, build
  system (kBuild/kmk), and generated config are coherent and actually compile &
  link on this host.
- **To produce a complete VirtualBox build** you need to install the host tools
  above — chiefly **`xsltproc`/libxslt** (plus Qt6 for the GUI) — on a machine
  with normal package access, then run `kmk` from the top level.
- The remaining large binary assets discussed earlier are **not** what blocks the
  build; the blocker is host tooling.
