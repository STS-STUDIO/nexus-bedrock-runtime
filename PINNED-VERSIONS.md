# Pinned versions — Nexus Bedrock Runtime v1.7.6-572-nexus6

> nexus6 (2026-07-15): imgui submodule initialized at its pinned commit
> a0bfbe4d8f6ddf7f678e6aeac7b1253fe2fc9cda (enables the overlay + Right-Shift Nexus
> mods page), TLS CA fix + crash-proof HTTP callbacks in mcpelauncher-client, and an
> httpclient RegisterNatives log in libjnivm. All local diffs re-captured below.

Exact source state of the tree that built the shipped runtime. Captured 2026-07-12 from the
build workspace (`~/nexus-engine/mcpelauncher`) with the commands shown below.

## Manifest repository

`git rev-parse HEAD`:

```
2af7c24b3d1e89cd1a594fb491f8c47372fd80cf
```

Branch `ng`, commit date 2026-06-25 15:24:46 +0000 ("Branch Sync").

`git remote -v`:

```
origin	https://github.com/minecraft-linux/mcpelauncher-manifest (fetch)
origin	https://github.com/minecraft-linux/mcpelauncher-manifest (push)
```

## Submodules

`git submodule status --recursive` (a leading space means the checkout matches the pinned
commit; `-` means not initialized). Submodules whose **working trees carried uncommitted local
changes** at build time are marked — those diffs are captured verbatim in
[`patches/local-*.patch`](patches/):

| | Pinned commit | Submodule | Local changes? |
|---|---|---|---|
| | `6c4fe11f8a4adaa0260a62cd6d5ed80f603f2933` | android-support-headers | |
| | `ba9f5e2e42fd2070fd7cfe7c10386ffdb544d208` | arg-parser | |
| | `e5d26109797c69d66260097b315446e774bc3639` | axml-parser | |
| | `683817e87a60c4befaef99f3d29589d3e17fc0e2` | base64 | |
| | `3f991cb324c80b1f80359058771becacf39835da` | cll-telemetry | |
| | `5b1f1bba4b360bba660070f6787af9b67985fae0` | daemon-utils | |
| | `c786200d744d02156ce02f70384784c942bc68bc` | eglut | |
| | `88650b21c97a6fffdffdc88545c178a2e690466f` | epoll-shim | |
| | `32da109b9e65331ed8931f40642b3fc6af9f7de5` | file-picker | |
| | `47193f42d10f0bb4bfcab021f305acb6f76192a0` | file-util | |
| **M** | `d073c2aace1521ae32d239ca4fc26b97e4b4a27e` | game-window | `patches/local-game-window.patch` |
| — | `a0bfbe4d8f6ddf7f678e6aeac7b1253fe2fc9cda` | imgui | not initialized (UI-only, `submodule.imgui.update none`; not built) |
| | `4ed0e447a38250435ec3c4fb596a53491465341c` | libc-shim | |
| | `f24b98c198fcc5c59a68d1efadd3f5791eb01c2e` | libjnivm | clean — stock upstream (see README note on historical patch 0001) |
| | `68d75a74f80a93ec4ff7a96eea0909df28d45330` | linux-gamepad | |
| | `6cd91de7228bb41f29efa6b0da0ec60ccb56397a` | logger | |
| | `b2f3760545450783771288144b56b3a2455b66e2` | mcpelauncher-apkinfo | |
| **M** | `a2f2a88a673d04f32fddcc36dc8b7c7eb663ee64` | mcpelauncher-client | `patches/local-mcpelauncher-client.patch` |
| | `d25a7e9e97a158c9bdec63947dd65b33d40dcedb` | mcpelauncher-common | |
| **M** | `f38ba5a1242b6b964588c1d2614583560cf9b068` | mcpelauncher-core | `patches/local-mcpelauncher-core.patch` |
| | `ee3a95d160736c9b62d7b46d105f6f063eb39bd0` | mcpelauncher-errorwindow | |
| | `1ac3ea6c1cf4f73a84d73f0bff510fba94a7d0f2` | mcpelauncher-linker | |
| **m** | `081b55b1f45745b1e6f93c49b3831107542a426d` | mcpelauncher-linker/bionic | `patches/local-mcpelauncher-linker-bionic.patch` (filesystem artifact, see patch header) |
| | `0235714fbf5593df145e8f991f82c5926c2df2df` | mcpelauncher-linker/core | |
| | `4eddeb017e0f5e374a29955ad9ccd9e0296f75dc` | mcpelauncher-linux-bin | |
| | `24794e34aaf11130022e915d0986e6116877cac4` | mcpelauncher-mac-bin | |
| | `cbd635c191184a9cec847dc67b473268a5222f30` | mcpelauncher-webview | |
| | `05e670dd609195a9bb3f150aaf1a065b84787316` | minecraft-imported-symbols | |
| | `ceb3857508d847cafe77283231634e4ea3c52713` | msa-daemon-client | |
| | `c08b775e59f84f6dec0afc143cac70d1b454116c` | osx-elf-header | |
| | `5210d2e139de53fb16146e35e785c92824006a27` | properties-parser | |
| | `483e79bf82fa9cbfcc7f35457c2f92817529d0e9` | sdl3 | |
| | `e71fdbdb8650454f04f798f35c8e33a9e1a18a6a` | simple-ipc | |

Notes on checkout:

- The nested submodules `mcpelauncher-linker/bionic` and `mcpelauncher-linker/core` are pinned to
  SHAs that cannot be fetched by SHA; clone their forks directly and `git checkout <sha>`:
  - bionic: `https://github.com/minecraft-linux/android_bionic` @ `081b55b1f45745b1e6f93c49b3831107542a426d`
  - core: `https://github.com/minecraft-linux/android_core` @ `0235714fbf5593df145e8f991f82c5926c2df2df`
- `imgui` was never initialized (`git config submodule.imgui.update none`) — it is UI-only and not
  referenced by the client build (`-DBUILD_UI=OFF`).

## Raw `git submodule status --recursive` output

```
 6c4fe11f8a4adaa0260a62cd6d5ed80f603f2933 android-support-headers (heads/master)
 ba9f5e2e42fd2070fd7cfe7c10386ffdb544d208 arg-parser (heads/master)
 e5d26109797c69d66260097b315446e774bc3639 axml-parser (heads/master)
 683817e87a60c4befaef99f3d29589d3e17fc0e2 base64 (heads/master)
 3f991cb324c80b1f80359058771becacf39835da cll-telemetry (heads/master)
 5b1f1bba4b360bba660070f6787af9b67985fae0 daemon-utils (heads/master)
 c786200d744d02156ce02f70384784c942bc68bc eglut (heads/master)
 88650b21c97a6fffdffdc88545c178a2e690466f epoll-shim (heads/master)
 32da109b9e65331ed8931f40642b3fc6af9f7de5 file-picker (heads/master)
 47193f42d10f0bb4bfcab021f305acb6f76192a0 file-util (heads/master)
 d073c2aace1521ae32d239ca4fc26b97e4b4a27e game-window (heads/master)
-a0bfbe4d8f6ddf7f678e6aeac7b1253fe2fc9cda imgui
 4ed0e447a38250435ec3c4fb596a53491465341c libc-shim (heads/master)
 f24b98c198fcc5c59a68d1efadd3f5791eb01c2e libjnivm (heads/main)
 68d75a74f80a93ec4ff7a96eea0909df28d45330 linux-gamepad (heads/master)
 6cd91de7228bb41f29efa6b0da0ec60ccb56397a logger (heads/master)
 b2f3760545450783771288144b56b3a2455b66e2 mcpelauncher-apkinfo (heads/master)
 a2f2a88a673d04f32fddcc36dc8b7c7eb663ee64 mcpelauncher-client (heads/master)
 d25a7e9e97a158c9bdec63947dd65b33d40dcedb mcpelauncher-common (heads/master)
 f38ba5a1242b6b964588c1d2614583560cf9b068 mcpelauncher-core (heads/master)
 ee3a95d160736c9b62d7b46d105f6f063eb39bd0 mcpelauncher-errorwindow (ee3a95d)
 1ac3ea6c1cf4f73a84d73f0bff510fba94a7d0f2 mcpelauncher-linker (heads/ng)
 081b55b1f45745b1e6f93c49b3831107542a426d mcpelauncher-linker/bionic (081b55b)
 0235714fbf5593df145e8f991f82c5926c2df2df mcpelauncher-linker/core (heads/main)
 4eddeb017e0f5e374a29955ad9ccd9e0296f75dc mcpelauncher-linux-bin (heads/master)
 24794e34aaf11130022e915d0986e6116877cac4 mcpelauncher-mac-bin (heads/v0.16.0)
 cbd635c191184a9cec847dc67b473268a5222f30 mcpelauncher-webview (heads/master)
 05e670dd609195a9bb3f150aaf1a065b84787316 minecraft-imported-symbols (heads/master)
 ceb3857508d847cafe77283231634e4ea3c52713 msa-daemon-client (heads/master)
 c08b775e59f84f6dec0afc143cac70d1b454116c osx-elf-header (heads/master)
 5210d2e139de53fb16146e35e785c92824006a27 properties-parser (heads/master)
 483e79bf82fa9cbfcc7f35457c2f92817529d0e9 sdl3 (483e79b)
 e71fdbdb8650454f04f798f35c8e33a9e1a18a6a simple-ipc (heads/master)
```

Other build inputs (not part of the mcpelauncher tree):

- **OpenSSL 3.2**, built from source with `perl Configure darwin64-arm64`; `libssl.3.dylib` and
  `libcrypto.3.dylib` ship inside the runtime bundle (OpenSSL is Apache-2.0-licensed).
- Host toolchain: standalone cmake + ninja binaries, Apple clang, native arm64 host build
  (no cross-compile flags), `-DBUILD_UI=OFF`.
