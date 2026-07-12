# ~/nexus-engine — rebuild instructions (workspace was deleted 2026-07-11)

This is the **build workspace** for Nexus's native-arm64 mcpelauncher engine. It was deleted along
with `~/nexus`. It is NOT needed for launcher development (that's `~/Downloads/nexus-antigravity`,
intact) and NOT needed to run the game (the compiled engine already ships in
`Nexus-Bedrock-Runtime.dmg`, runtime tag `v1.7.6-572-nexus5`). Rebuild this **only when you want to
change the engine again** (e.g. the "unknown image type" texture-decoder fix).

## The compiled engine is already recoverable (no rebuild needed to run)
- Installed bundle: `~/Library/Application Support/NexusLauncher/bedrock-runtime/Minecraft Bedrock.app`
- Or download `Nexus-Bedrock-Runtime.dmg` from the release channel and mount it.

## Full rebuild (to modify + re-ship the engine)
Full recipe + every patch is in `~/nexus/docs/engine-build-plan.md`. Summary:

1. **Toolchain** (no Homebrew/sudo): download cmake + ninja binaries into `~/nexus-engine/toolchain/`.
2. **Source**: `git clone -b ng --recursive https://github.com/minecraft-linux/mcpelauncher-manifest \
   ~/nexus-engine/mcpelauncher`
   - `git config submodule.imgui.update none` (UI-only, unfetchable pin).
   - Nested `mcpelauncher-linker/{bionic,core}`: clone the forks directly + `git checkout <sha>`.
3. **OpenSSL 3.2**: `perl Configure darwin64-arm64` → `~/nexus-engine/openssl-install`.
4. **Apply patches** (`~/nexus/docs/engine-patches/`): 0006 (64 MB HTTP-worker stack — the M5 crash
   fix), 0007 (headless Google credentials), 0003 (ES 3.1), 0001 (jnivm GetEnv lock). These are small
   and hand-appliable.
5. **Build client**: `~/nexus-engine/build-client/` → `ninja mcpelauncher-client` (host arm64, no
   cross-compile flags; client `needs: openssl`, `-DBUILD_UI=OFF`).
6. **Package**: `~/nexus-engine/package-runtime.sh` (see the reconstructed skeleton next to this file)
   — swaps the patched client into a copy of the installed bundle, ships OpenSSL dylibs, fixes
   @rpath with install_name_tool, re-signs.
7. **Publish**: from `~/Downloads/nexus-antigravity`, run `publish-runtime-patched.sh` with a bumped
   `TAG` (e.g. `v1.7.6-572-nexus6`) — clients then re-download it.

When you want to do this, just say so and I'll drive the whole rebuild as a background job.
