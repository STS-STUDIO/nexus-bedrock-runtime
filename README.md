# Nexus Bedrock Runtime — corresponding source

This repository is the **corresponding source code** (GNU GPL-3.0, section 6) for the
**Nexus Bedrock Runtime** — the `Nexus-Bedrock-Runtime.dmg` that the
[Nexus launcher](https://stsstudio.org/nexus) by STS Studio downloads and runs on macOS
(Apple Silicon).

The runtime is a build of the open-source **mcpelauncher** project
([github.com/minecraft-linux/mcpelauncher-manifest](https://github.com/minecraft-linux/mcpelauncher-manifest),
GPL-3.0), **modified by STS Studio on 2026-07-11** with a small set of macOS/Apple-Silicon
compatibility patches, and compiled natively for arm64. This repository documents the exact
upstream commits and carries every modification, so anyone can reproduce the shipped binary's
source tree.

- **License: GNU General Public License v3.0** — see [LICENSE](LICENSE) (copied verbatim from the
  upstream mcpelauncher-manifest tree). The patches in this repository are likewise provided
  under GPL-3.0.
- The Nexus **launcher** itself is a separate, proprietary application; it runs this runtime as
  an independent process. The game (Minecraft Bedrock) is **never** distributed by STS Studio —
  each user's copy is downloaded from their own Google Play account.

## Release ↔ source mapping

| Runtime tag (shipped DMG) | Upstream base (mcpelauncher-manifest, branch `ng`) | Patches |
|---|---|---|
| `v1.7.6-572-nexus5` | `2af7c24b3d1e89cd1a594fb491f8c47372fd80cf` (2026-06-25) | all of `patches/local-*.patch` (this repo's tag `v1.7.6-572-nexus5`) |

Future runtime releases will add a row here and a matching git tag on the commit that describes
them.

## How the source tree is reconstructed

1. Clone the pinned upstream:

   ```sh
   git clone -b ng --recursive https://github.com/minecraft-linux/mcpelauncher-manifest
   cd mcpelauncher-manifest
   git checkout 2af7c24b3d1e89cd1a594fb491f8c47372fd80cf
   git submodule update --init --recursive   # see PINNED-VERSIONS.md for the fixups
   ```

   Two nested submodules (`mcpelauncher-linker/bionic`, `mcpelauncher-linker/core`) are pinned to
   commits that cannot be fetched by SHA — clone
   `minecraft-linux/android_bionic` / `minecraft-linux/android_core` directly and check out the
   SHAs listed in [PINNED-VERSIONS.md](PINNED-VERSIONS.md). The `imgui` submodule is skipped
   (UI-only; the client builds with `-DBUILD_UI=OFF`).

2. Apply the STS Studio modifications. The **authoritative, machine-appliable** diffs are the
   `patches/local-*.patch` files — each is the verbatim `git diff` of a submodule's working tree
   at build time, applied on the pinned commit named in its header:

   | Patch file | Submodule | What it changes |
   |---|---|---|
   | `local-game-window.patch` | game-window | **0003** — request OpenGL ES **3.1** instead of 3.0 (`src/window_glfw.cpp`); MC 1.26+ requires ES 3.1. |
   | `local-mcpelauncher-client.patch` | mcpelauncher-client | **0006** — run the HTTP completion worker on a 64 MB-stack pthread (`src/jni/lib_http_client.cpp`); fixes the Apple-Silicon SIGSEGV (512 KB default stack overflow). **0003b** — wire up the mvk-angle EGL library for all game versions, not just 1.26.10+ (`src/main.cpp`). |
   | `local-mcpelauncher-core.patch` | mcpelauncher-core | **0007** — headless Google credentials: read `nexus-google-cred` ("email:token") written by the launcher instead of forking the `mcpelauncher-ui-qt` credential window (`src/minecraft_utils.cpp`). |
   | `local-mcpelauncher-linker-bionic.patch` | mcpelauncher-linker/bionic | **Not an intentional patch** — a macOS case-insensitive-filesystem checkout artifact (`xt_MARK.h` vs `xt_mark.h` etc. collide). Recorded so this repo reproduces the build tree byte-for-byte; these Linux-kernel netfilter headers are not compiled into the macOS build. |

   ```sh
   git -C game-window          apply ../patches/local-game-window.patch
   git -C mcpelauncher-client  apply ../patches/local-mcpelauncher-client.patch
   git -C mcpelauncher-core    apply ../patches/local-mcpelauncher-core.patch
   # optional, artifact only:
   git -C mcpelauncher-linker/bionic apply ../patches/local-mcpelauncher-linker-bionic.patch
   ```

   The `patches/0003b-*.patch`, `patches/0006-*.patch` and `patches/0007-*.patch` files are the
   original annotated/hand-appliable write-ups of the same changes, kept for their explanatory
   headers; the `local-*.patch` files supersede them for applying.

3. Build and package — see [build/REBUILD.md](build/REBUILD.md). In short: standalone cmake+ninja
   toolchain, OpenSSL 3.2 (`perl Configure darwin64-arm64`), `ninja mcpelauncher-client` with
   `-DBUILD_UI=OFF` (native arm64 host build, no cross-compile flags), then
   [build/package-runtime.sh](build/package-runtime.sh) swaps the patched client into the runtime
   bundle, ships the OpenSSL dylibs, fixes `@rpath`, and re-signs.
   [build/publish-runtime-patched.sh](build/publish-runtime-patched.sh) publishes the DMG to the
   release channel (its upload secret is read from the environment, never stored).

## Note on the historical "0001 jnivm GetEnv lock" patch

Older Nexus build notes list a fourth patch — a mutex lock in `libjnivm`'s `VM::GetEnv()`, used
during early crash diagnosis (it did not fix the crash; patch 0006 did). **The shipped
`v1.7.6-572-nexus5` build does not contain it**: in the build workspace the `libjnivm` submodule
is a pristine checkout of upstream commit `f24b98c198fcc5c59a68d1efadd3f5791eb01c2e`
(clean working tree, no local commits, source files untouched since clone — before the build
ran). This repository therefore correctly ships **stock upstream libjnivm** as part of the
corresponding source for this release.

## Exact source-state record

[PINNED-VERSIONS.md](PINNED-VERSIONS.md) lists the manifest HEAD, remotes, and every submodule
SHA (`git submodule status --recursive`) of the tree that produced the shipped binary, with the
dirty submodules marked.

---

Copyright notice for the modifications: © 2026 STS Studio. This program is free software: you
can redistribute it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License, or (at your option)
any later version. It is distributed WITHOUT ANY WARRANTY; see [LICENSE](LICENSE) for details.
