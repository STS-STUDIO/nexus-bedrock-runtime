# ~/nexus-engine rebuild instructions

This is the **build workspace** for Nexus's native-arm64 mcpelauncher engine.

**Status: restored and verified end to end on 2026-08-30.** The whole recipe below was executed from
scratch on macOS 27.0 (Apple M5), the client built clean, and 1.26.10.4 launched to a fully rendered
main menu. Every step here is what actually ran, not a reconstruction.

It is NOT needed for launcher development (that's `~/Downloads/nexus-antigravity`) and NOT needed to
run the game (the compiled engine ships in `Nexus-Bedrock-Runtime.dmg`). Rebuild it when you want to
change the engine, or to regenerate the fork patches this mirror publishes.

## STOP: this tree is nexus6. What ships is nexus8. Do not build and ship without reading this.

The restored workspace reproduces **v1.7.6-572-nexus6** exactly. The runtime installed on players'
machines is **v1.7.6-572-nexus8**, and the nexus7/nexus8 source is **not in this tree and not
anywhere else on this machine** (searched 2026-08-30: the only `imgui_ui.cpp` on disk is the nexus6
one here).

The gap is real and it is large:

| | nexus6 build from this tree | shipped nexus8 binary |
|---|---|---|
| size | 9,175,712 bytes | 10,254,208 bytes |
| `nexus_*` settings | **0** | **16** |

The 16 features present only in the shipped binary, recovered with `strings`:

```
nexus_auto_checkout   nexus_cps            nexus_cps_pos         nexus_crosshair
nexus_discord_button  nexus_fps            nexus_fps_pos         nexus_keystrokes
nexus_keystrokes_pos  nexus_real_clock     nexus_real_clock_pos  nexus_session_timer
nexus_session_timer_pos  nexus_toggle_sprint  nexus_watermark    nexus_watermark_pos
```

`~/Library/Application Support/mcpelauncher/mcpelauncher-client-settings.txt` already has values
saved for these, so players are using them today.

**Therefore: building from this tree and shipping the result would silently delete the entire Nexus
HUD (keystrokes, CPS, FPS, watermark, crosshair, session timer, real clock, toggle-sprint, Discord
button).** This is the same trap as the NexusCore and 26.2-client source losses. Until the nexus7/8
work is recovered or rewritten, treat this workspace as **read-only for shipping**: use it to study
the engine, to regenerate the nexus6 patches, and to test builds locally. Do not swap its output
into the installed runtime and do not publish a tag from it.

### Where the gap actually is, from a string diff of the three bundles

The old bundle backups next to the installed one are the intermediate tags, so the delta can be
localised without the source. Comparing sorted `strings` output:

- `Minecraft Bedrock.app.fat-backup` (Jul 14, 7,799,472 B) is pre-nexus6.
- `Minecraft Bedrock.app.pre-imgui` (Jul 14, 8,323,648 B) is nexus5.
- `Minecraft Bedrock.app.pre-nexus8` (Jul 17, 10,185,488 B) is **nexus7**.
- `Minecraft Bedrock.app` (Aug 28, 10,254,208 B) is **nexus8**, the live one.

**nexus7 adds 964 strings over the nexus6 build**, but almost nothing recognisably Nexus: the only
notable ones are a build-path leak and `Nexus Launcher %s / build %s`. Whatever nexus7 changed, it
is not a large user-facing feature.

**nexus8 adds 358 strings over nexus7, and that is the whole missing feature set.** It is an imgui
mods UI with a module system, plus launcher-account integration:

```
##nexusgrid   ##nexusmodsettings   ##nexussearch   ##nexusdiscordbtn   ##joindiscord
/Library/Application Support/NexusLauncher/account.json
"HUD POSITION"   "Keystrokes"   "Discord Button"   "NEXUS branding on your screen"
"Draws a thin Nexus cross at the exact centre of the screen."
"Bedrock HUD modules snap to corner presets and stack inside their corner. Free drag
 placement, per module scale and the Java live preview are not wired up in this engine yet."
"Gameplay module - it changes how the game behaves and has no HUD box."
```

So a rewrite target is: a searchable module grid in `imgui_ui.cpp`, per-module settings with
corner-preset HUD positioning, the 16 `nexus_*` keys persisted through the existing launcher
settings file, and reading `account.json` from the launcher's support dir. Recovering the real
nexus8 source is still far preferable to reimplementing from these strings.

## The compiled engine is also recoverable without a rebuild
- Installed bundle: `~/Library/Application Support/NexusLauncher/bedrock-runtime/Minecraft Bedrock.app`
- Or download `Nexus-Bedrock-Runtime.dmg` from the release channel and mount it.

---

## Corrections to the previous version of this file

The 2026-07-12 text was written after the workspace was deleted and carried several wrong claims.
All of these are fixed below; recorded here so nobody re-derives them.

1. **imgui is NOT "unfetchable".** It clones normally from `https://github.com/ocornut/imgui` at the
   `.gitmodules` pin `a0bfbe4d8f6ddf7f678e6aeac7b1253fe2fc9cda` (dated 2025-11-05). Do **not** set
   `git config submodule.imgui.update none`. That was the old advice and it is wrong. Since nexus6
   imgui is **required**: the client compiles `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
   `imgui_widgets.cpp`, `imgui_demo.cpp` plus our `src/imgui_ui.cpp`. `-DBUILD_UI=OFF` switches off
   the separate **Qt** UI and has nothing to do with imgui.
2. **`~/nexus/docs/engine-build-plan.md` and `~/nexus/docs/engine-patches/` do not exist.** `~/nexus`
   was deleted and never restored. The patches live in this repo, in `../patches/`.
3. **Do not hand-apply the numbered patches 0003 / 0003b / 0006 / 0007.** They are already folded
   into the four `local-*.patch` files in `../patches/`, which are full `git diff` output and apply
   with `git apply`. Applying both would conflict or double-apply. The numbered files are kept as
   prose explanations of *why* each change exists; read them, do not run them.
4. **The nested `mcpelauncher-linker/{bionic,core}` submodules do not need manual cloning.** Plain
   `--recursive` fetches both at the pinned SHAs. The old "cannot be fetched by SHA, clone the forks
   directly" claim did not reproduce.
5. **OpenSSL was unpinned.** It is a branch checkout, not a release tarball, so "OpenSSL 3.2" alone
   does not reproduce. The exact commit and the exact `Configure` line are recorded in step 3.

---

## Full rebuild

Timings below are from the 2026-08-30 run on an M5.

### 1. Toolchain (no Homebrew, no sudo)

Standalone binaries in `~/nexus-engine/toolchain/`:

- cmake 3.31.6 (macos-universal tarball, unpacked to `toolchain/CMake.app`, symlinked as
  `toolchain/cmake`)
- ninja 1.12.1 (`ninja-mac.zip`)

Everything below assumes `export PATH="$HOME/nexus-engine/toolchain:$PATH"`.

### 2. Source

```sh
git clone -b ng --recursive \
  https://github.com/minecraft-linux/mcpelauncher-manifest ~/nexus-engine/mcpelauncher
```

Then check out the pinned SHAs. `../PINNED-VERSIONS.md` has the full table and the raw
`git submodule status --recursive` output to diff against. All 34 entries must match, imgui included.

**Expected dirt that is not a patch:** `mcpelauncher-linker/bionic` shows 8 modified files under
`libc/kernel/uapi/linux/netfilter*/`:

```
xt_CONNMARK.h  xt_DSCP.h  xt_MARK.h  xt_RATEEST.h  xt_TCPMSS.h
ipt_ECN.h  ipt_TTL.h  ip6t_HL.h
```

These are a **case-insensitive-filesystem artifact**. Each has a lowercase sibling in the same
directory (`xt_connmark.h` etc.), and APFS collapses the pair, so git sees the wrong content
checked out. Leave them alone. They do not affect the build.

### 3. OpenSSL 3.2

```sh
git clone -b openssl-3.2 https://github.com/openssl/openssl.git ~/nexus-engine/openssl
cd ~/nexus-engine/openssl
git checkout 14ddbcee237cb99b3921c352852b4d4fadbb8e6c    # 2025-11-24, branch openssl-3.2
perl Configure darwin64-arm64 \
  --prefix="$HOME/nexus-engine/openssl-install" \
  --openssldir=/etc/ssl \
  no-tests
make -j"$(sysctl -n hw.ncpu)" && make install_sw
```

Produces `openssl-install/lib/lib{ssl,crypto}.3.dylib`, reporting `OpenSSL 3.2.7-dev`. Apache-2.0.

`--openssldir=/etc/ssl` matters: it points OpenSSL's default trust store at the system one. The
client separately forces `CURLOPT_CAINFO=/etc/ssl/cert.pem` (`mcpelauncher-client/src/jni/lib_http_client.cpp`),
which is the load-bearing half. Get either wrong and every in-game HTTPS request fails silently.

### 4. Apply the fork patches

From `~/nexus-engine/mcpelauncher`:

```sh
P=~/Desktop/nexus-bedrock-runtime/patches
(cd game-window          && git apply "$P/local-game-window.patch")
(cd libjnivm             && git apply "$P/local-libjnivm.patch")
(cd mcpelauncher-client  && git apply "$P/local-mcpelauncher-client.patch")
(cd mcpelauncher-core    && git apply "$P/local-mcpelauncher-core.patch")
```

Verify each applied cleanly by diffing back:

```sh
for d in game-window libjnivm mcpelauncher-client mcpelauncher-core; do
  (cd "$d" && git diff) | diff -q - "$P/local-$d.patch" && echo "$d OK"
done
```

(`local-mcpelauncher-linker-bionic.patch` is only the record of the case-collision artifact in
step 2. It is not applied.)

### 5. Configure + build the client

```sh
export PATH="$HOME/nexus-engine/toolchain:$PATH"
cmake -G Ninja -S ~/nexus-engine/mcpelauncher -B ~/nexus-engine/build-client \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_UI=OFF \
  -DBUILD_WEBVIEW=OFF \
  -DENABLE_DEV_PATHS=OFF \
  -DOPENSSL_ROOT_DIR="$HOME/nexus-engine/openssl-install" \
  -DOPENSSL_INCLUDE_DIR="$HOME/nexus-engine/openssl-install/include" \
  -DOPENSSL_SSL_LIBRARY="$HOME/nexus-engine/openssl-install/lib/libssl.3.dylib" \
  -DOPENSSL_CRYPTO_LIBRARY="$HOME/nexus-engine/openssl-install/lib/libcrypto.3.dylib"

cd ~/nexus-engine/build-client && ninja mcpelauncher-client
```

Host arm64, no cross-compile flags. Configure takes about 100 s and **needs network**: it downloads
curl 8.0.1, glfw3 and nlohmann_json at configure time. The build itself is 457 targets, about 2 minutes
on an M5 from a cold build directory (re-measured 2026-08-30; the older "33 s" figure was an
incremental build, not a clean one).

Expected, harmless: OpenSSL 3.0-deprecation warnings, one `-Wnontrivial-memcall` in `imgui_ui.cpp`,
a duplicate `-lpthread` note, and `ld: building for macOS-26.5 but linking with dylib ... built for
newer version 27.0` for both OpenSSL dylibs. Also a Qt AUTOGEN warning for
`mcpelauncher-errorwindow`, which is expected with `BUILD_UI=OFF` and irrelevant (that binary is
dead weight in the shipped bundle anyway).

Output: `~/nexus-engine/build-client/mcpelauncher-client/mcpelauncher-client`, arm64 Mach-O.

**The build is reproducible.** Deleting `build-client` entirely and re-running the two commands
above produced a byte-identical binary (sha256
`121449aa297cdf454e81e6aa6db74721f2104a44fd6a9046537cf59b1e63cbfd`, 9,175,712 bytes, for the
nexus6 source state).

### 6. Package

```sh
TAG=v1.7.6-572-nexusN bash ~/Desktop/nexus-bedrock-runtime/build/package-runtime.sh
```

**This step overwrites `~/Downloads/nexus-antigravity/runtime-release/Nexus-Bedrock-Runtime.dmg`,
the artefact the publish script uploads.** Because this tree is nexus6 and the release DMG holds the
nexus8 client, running it unguarded replaces the reviewed release candidate with a build that has no
Nexus HUD. The script now refuses the DMG step when the client it just swapped in has fewer
`nexus_*` settings than the installed runtime, and tells you to set `ALLOW_HUD_REGRESSION=1` if you
really mean it. The bundle at `~/nexus-engine/patched-runtime` is still produced either way, which
is what the launch test in step 7 uses.

Copies the installed bundle to `~/nexus-engine/patched-runtime/`, swaps in the fresh client, drops
the Qt payload, ships `nexus-webview` and the two OpenSSL dylibs, rewrites their linkage to
`@rpath`, and ad-hoc re-signs. It reads `~/Downloads/nexus-antigravity/src-tauri/resources/nexus-webview`,
so build that in the launcher repo first. It never touches the installed runtime.

### 7. Launch test before shipping anything

Never swap the installed runtime without this passing.

```sh
APP="$HOME/nexus-engine/patched-runtime/Minecraft Bedrock.app"
D="$HOME/Library/Application Support/mcpelauncher"
MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=0 \
PATH="$APP/Contents/MacOS:$PATH" \
"$APP/Contents/MacOS/mcpelauncher-client-arm64-v8a" \
  -dg "$D/versions/1.26.10.4" \
  -m  "$D/mods/mcpelauncher-updates/1.26.10.4/arm64-v8a/"
```

Use **1.26.10.4**. 1.26.20.4 segfaults in the helper mod. The `-m` flag is load bearing: without it
the DRM library segfaults. On macOS < 14 also set `ANGLE_DEFAULT_PLATFORM=metal` or it black-screens.

A healthy run logs, in order:

```
[Launcher] Game version: 1.26.10.4
[ModLoader] Loaded 1 mods
[MinecraftUtils] Failed to load Minecraft: dlopen failed: cannot locate symbol "glBindRenderbuffer"
[Launcher] Creating window
[mvk-info] Created VkDevice to run on GPU Apple M5
[GL] Version: OpenGL ES 3.1 (ANGLE 2.1.44625 ...)
```

The `glBindRenderbuffer` error is **normal and expected**, not a failure. Desktop GL is tried first,
fails, and the client falls back to the ES/EGL path through mvk-angle. That fallback is what patch
0003b widened to all versions; if it ever stops happening, older versions abort at window creation
with "EGL: Library not found".

Two more expected non-failures:

- Hundreds of `Image failed to load from memory / Reason: unknown image type` warnings. This is the
  known texture-decoder gap. It does **not** prevent rendering; the 2026-08-30 test reached a
  complete main menu (panorama, logo, Play/Settings/Realms/Marketplace, skins, Dressing Room) with
  these warnings streaming.
- `EventsSDK.SQLiteDB / Failed to open database file` and the telemetry noise after it. Cosmetic.

Proof that TLS is actually working: the log should show real HTTP responses from Mojang, e.g.
`GatheringServiceRequest: /api/v1.0/config/public?... [404]` with a JSON body. A silent absence of
those means the CA bundle is broken (see step 3).

### 8. Publish

From `~/Downloads/nexus-antigravity`, run `publish-runtime-patched.sh` with a bumped `TAG`
(e.g. `v1.7.6-572-nexus9`). **Publish the corresponding source to this mirror and tag it with the
same tag in the same wave**, because the shipped runtime contains GPL-3.0 binaries and the mirror is how
that obligation is met.
