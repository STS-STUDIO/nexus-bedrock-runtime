# Nexus Bedrock Runtime — corresponding source

This repository is the **corresponding source code** (GNU GPL-3.0, section 6) for the
**Nexus Bedrock Runtime** — the `Nexus-Bedrock-Runtime.dmg` that the
[Nexus launcher](https://stsstudio.org/nexus) by STS Studio downloads and runs on macOS
(Apple Silicon).

The runtime is a build of the open-source **mcpelauncher** project
([github.com/minecraft-linux/mcpelauncher-manifest](https://github.com/minecraft-linux/mcpelauncher-manifest),
GPL-3.0), **modified by STS Studio during 2026** with a set of macOS/Apple-Silicon compatibility
patches, and compiled natively for arm64. This repository documents the exact
upstream commits and carries every modification, so anyone can reproduce the shipped binary's
source tree.

- **License: GNU General Public License v3.0** — see [LICENSE](LICENSE) (copied verbatim from the
  upstream mcpelauncher-manifest tree). The patches in this repository are likewise provided
  under GPL-3.0.
- [NOTICE.txt](NOTICE.txt) is the notice shipped on the runtime download channel: it names this
  repository as the corresponding source and carries the written offer of source. It is a
  template: the publishing scripts substitute the release tag for `__RUNTIME_TAG__` so the served
  notice always names the build people actually hold. It is identical to
  [build/runtime-notice.txt](build/runtime-notice.txt), which is the copy the scripts read; the
  authoritative copy lives beside those scripts in the launcher repository.
- The Nexus **launcher** itself is a separate, proprietary application; it runs this runtime as
  an independent process. The game (Minecraft Bedrock) is **never** distributed by STS Studio —
  each user's copy is downloaded from their own Google Play account.

## Release ↔ source mapping

| Runtime tag (shipped DMG) | Upstream base (mcpelauncher-manifest, branch `ng`) | Corresponding source |
|---|---|---|
| `v1.7.6-572-nexus5` | `2af7c24b3d1e89cd1a594fb491f8c47372fd80cf` (2026-06-25) | complete, at this repo's tag `v1.7.6-572-nexus5` |
| `v1.7.6-572-nexus6` | `2af7c24b3d1e89cd1a594fb491f8c47372fd80cf` (2026-06-25) | complete, at this repo's tag `v1.7.6-572-nexus6` |
| `v1.7.6-572-nexus7` | same | **NOT IN THIS REPOSITORY** (see below) |
| `v1.7.6-572-nexus8` | same | **NOT IN THIS REPOSITORY** (see below); this is the release live on the download channel |
| `v1.7.6-572-nexus9` | same | **NOT IN THIS REPOSITORY** (see below); prepared, not yet published |

Every release gets a row here and a git tag of the same name on the commit that carries its
patches. Both publishing scripts (`build/publish-runtime-patched.sh` and `build/mirror-runtime.sh`)
now call `require_mirrored_source`, which does a `git ls-remote` against this repository and exits
before uploading anything if the tag is not here. That is what stops the table silently falling
behind again.

### Releases whose source is not here yet

`v1.7.6-572-nexus7` (2026-07-16) and `v1.7.6-572-nexus8` (2026-07-18) were built and published
without their patches being mirrored here, and the build workspace they were built in
(`~/nexus-engine`) was later deleted. Their modifications, over `nexus6`, were:

- **nexus7**: Xbox sign in fix (the webview lookup searched `$PATH` only; the build now sets
  `XAL_WEBVIEW_QT_PATH="."` so it searches the app directory), the Nexus overlay mod page in
  `imgui_ui.cpp`, and the overlay mods Watermark / Session Timer / Real Clock / Custom Crosshair.
- **nexus8**: overlay HUD modules (FPS, CPS, Keystrokes, Session Timer) with per corner placement,
  a toggle sprint key helper, a clickable Discord button, and a STORE tab that reads the Nexus
  catalogue over HTTPS and hands a purchase intent to the launcher.

`v1.7.6-572-nexus9` carries **the same client binary as `nexus8`**: it is a repackage that drops
dead weight from the bundle, not a rebuild. Its GPL binaries are byte for byte the `nexus8` ones,
so it inherits the same missing source.

Until that source exists here, this repository is the corresponding source for `nexus5` and
`nexus6` only, and anyone holding `nexus7`, `nexus8` or `nexus9` should use the source request
address in [NOTICE.txt](NOTICE.txt), which now says so in the notice itself rather than leaving
the reader to discover it.

#### What was checked, so nobody repeats the search

Restoring the deleted `~/nexus-engine` workspace does **not** bring this source back. Checked on
2026-08-30:

- The restored workspace checks out at the `nexus6` patch level. Its
  `mcpelauncher-client/src/imgui_ui.cpp` contains only the Right-Shift Nexus page and the
  Keystrokes + CPS HUD, which are already in `patches/local-mcpelauncher-client.patch`.
- The shipped `nexus8` client binary
  (`sha256 4ffd89217a242fc88ed7f04ce3b69d0265ee1cbe1cf9e093859d94ef00574cde`) carries settings keys
  and UI strings that exist nowhere in that tree: `nexus_watermark`, `nexus_real_clock`,
  `nexus_session_timer`, `nexus_crosshair`, `nexus_toggle_sprint`, `nexus_discord_button`,
  `nexus_fps` / `nexus_cps` / `nexus_keystrokes` with `_pos` variants, and a `STORE` tab.
- No source file anywhere on the build machine contains those identifiers, there is no Time Machine
  destination configured, and there are no local APFS snapshots to recover from.

So the two honest routes are: rewrite those features against the restored tree and ship a release
whose source **is** published, which supersedes `nexus7` and `nexus8` for every player through
auto-update; or keep serving the written offer in [NOTICE.txt](NOTICE.txt) and answer it by hand.
The first route is the one that actually ends the obligation. Publishing `nexus9` as-is does not:
it ships the same un-sourced binary under a new tag.

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
   SHAs listed in [PINNED-VERSIONS.md](PINNED-VERSIONS.md). The `imgui` submodule clones normally
   from `github.com/ocornut/imgui` at the `.gitmodules` pin
   `a0bfbe4d8f6ddf7f678e6aeac7b1253fe2fc9cda`; since `nexus6` the in game overlay is compiled in,
   so do initialize it. (`-DBUILD_UI=OFF` switches off the separate Qt user interface, not imgui.)

2. Apply the STS Studio modifications. The **authoritative, machine-appliable** diffs are the
   `patches/local-*.patch` files — each is the verbatim `git diff` of a submodule's working tree
   at build time, applied on the pinned commit named in its header:

   | Patch file | Submodule | What it changes |
   |---|---|---|
   | `local-game-window.patch` | game-window | **0003** — request OpenGL ES **3.1** instead of 3.0 (`src/window_glfw.cpp`); MC 1.26+ requires ES 3.1. |
   | `local-mcpelauncher-client.patch` | mcpelauncher-client | **0006** — run the HTTP completion worker on a 64 MB-stack pthread (`src/jni/lib_http_client.cpp`); fixes the Apple-Silicon SIGSEGV (512 KB default stack overflow). **0003b** — wire up the mvk-angle EGL library for all game versions, not just 1.26.10+ (`src/main.cpp`). Since `nexus6` also: `CURLOPT_CAINFO=/etc/ssl/cert.pem` on both curl handles (the bundled OpenSSL ships no CA bundle, so every in game HTTPS request failed TLS verification), guarded HTTP completion callbacks, and the Right-Shift Nexus overlay page (`src/imgui_ui.cpp`). |
   | `local-libjnivm.patch` | libjnivm | Since `nexus6`: log `RegisterNatives` registrations for the httpclient classes (`src/jnivm/vm.cpp`), used to diagnose the HTTP callback crash. |
   | `local-mcpelauncher-core.patch` | mcpelauncher-core | **0007** — headless Google credentials: read `nexus-google-cred` ("email:token") written by the launcher instead of forking the `mcpelauncher-ui-qt` credential window (`src/minecraft_utils.cpp`). |
   | `local-mcpelauncher-linker-bionic.patch` | mcpelauncher-linker/bionic | **Not an intentional patch** — a macOS case-insensitive-filesystem checkout artifact (`xt_MARK.h` vs `xt_mark.h` etc. collide). Recorded so this repo reproduces the build tree byte-for-byte; these Linux-kernel netfilter headers are not compiled into the macOS build. |

   ```sh
   git -C game-window          apply ../patches/local-game-window.patch
   git -C mcpelauncher-client  apply ../patches/local-mcpelauncher-client.patch
   git -C mcpelauncher-core    apply ../patches/local-mcpelauncher-core.patch
   git -C libjnivm             apply ../patches/local-libjnivm.patch
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
corresponding source for that release. From `nexus6` onward `libjnivm` does carry a local change,
but it is a different one: the `RegisterNatives` logging in `patches/local-libjnivm.patch`, not
the `GetEnv` lock.

## Exact source-state record

[PINNED-VERSIONS.md](PINNED-VERSIONS.md) lists the manifest HEAD, remotes, and every submodule
SHA (`git submodule status --recursive`) of the tree that produced the shipped binary, with the
dirty submodules marked.

---

Copyright notice for the modifications: © 2026 STS Studio. This program is free software: you
can redistribute it and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation, either version 3 of the License, or (at your option)
any later version. It is distributed WITHOUT ANY WARRANTY; see [LICENSE](LICENSE) for details.
