#!/bin/bash
# =============================================================================
# Nexus - package the PATCHED Bedrock runtime bundle.
# Reconstructed 2026-07-11 (workspace was deleted). Swaps our freshly-built patched
# mcpelauncher-client into a copy of the installed runtime bundle, ships the OpenSSL
# dylibs, fixes @rpath, strips what we have no right or no reason to ship, drops in the
# licence notice, re-signs, and builds the DMG.
#
#   TAG=v1.7.6-572-nexus9 ./package-runtime.sh
#
# Output: the bundle at ~/nexus-engine/patched-runtime, and the DMG at
# ~/Downloads/nexus-antigravity/runtime-release/Nexus-Bedrock-Runtime.dmg.
# Then publish it with publish-runtime-patched.sh using the SAME TAG.
#
# EVERY removal in the STRIP block below is deliberate and was launch-tested. The nexus9
# strip was first done by hand on an unpacked DMG, which meant the next run of this script
# would have put FMOD and the dead x86_64 payload straight back into the next release.
# That is the whole reason it lives here now. Do not "tidy" it away.
# =============================================================================
set -euo pipefail
say() { echo "== $*"; }

TAG="${TAG:-}"                 # stamps the licence notice inside the bundle
WS="$HOME/nexus-engine"
SRC="$HOME/Library/Application Support/NexusLauncher/bedrock-runtime/Minecraft Bedrock.app"
DST="$WS/patched-runtime/Minecraft Bedrock.app"
CLIENT="$WS/build-client/mcpelauncher-client/mcpelauncher-client"     # freshly built patched client
OPENSSL="$WS/openssl-install/lib"

[ -d "$SRC" ]   || { echo "No source bundle at: $SRC (install/run Bedrock once first)"; exit 1; }
[ -f "$CLIENT" ] || { echo "No built client at: $CLIENT (build it: ninja mcpelauncher-client)"; exit 1; }

say "copying bundle…"
rm -rf "$WS/patched-runtime"; mkdir -p "$WS/patched-runtime"
ditto "$SRC" "$DST"

say "swapping in the patched arm64 client…"
cp "$CLIENT" "$DST/Contents/MacOS/mcpelauncher-client-arm64-v8a"

# ── SLIM: drop the dead Qt payload (~227 MB of the 401 MB bundle) ────────────────────────────
# The launch path (mcpelauncher-client-arm64-v8a, direct exec) is Qt-free — verified via otool.
# Qt only served: mcpelauncher-ui-qt (x86_64 first-run fallback Nexus never reaches — the
# launcher downloads the game itself), msa-ui-qt (legacy MSA UI), and the stock Qt webview
# (replaced by nexus-webview below). msa-daemon is kept, it is still used; the
# x86_64 clients and the crash helper are dropped by the STRIP block below.
say "slimming: removing Qt frameworks, plugins, qml, ui-qt, msa-ui-qt…"
rm -rf "$DST/Contents/Frameworks/"Qt*.framework
rm -rf "$DST/Contents/Frameworks/Sparkle.framework"
rm -rf "$DST/Contents/Frameworks/__MACOSX"
rm -rf "$DST/Contents/PlugIns"
rm -rf "$DST/Contents/Resources/qml"
rm -f  "$DST/Contents/Resources/qt.conf"
rm -f  "$DST/Contents/MacOS/mcpelauncher-ui-qt"
rm -f  "$DST/Contents/MacOS/msa-ui-qt"

# -- STRIP: what we have no right to ship, and what nothing can reach -------------------------
# All four groups were verified against a launch test of 1.26.10.4 on the arm64 path: menu
# rendered, audio live on CoreAudio, HTTPS established to the Xbox Live endpoints.
#
#  1. FMOD.  lib/native/{arm64-v8a,x86_64}/libfmod.dylib is genuine Firelight Technologies
#     FMOD (1,194 FMOD5_* exports). Shipping it inside a product that sells ranks and gems
#     needs a paid commercial licence and we do not have one. It is safe to drop because the
#     host copy is OPTIONAL. minecraft_utils.cpp:loadFMod dlopens it through
#     PathHelper::findDataFile, main.cpp catches the failure, and then
#     linker::dlopen("libfmod.so") picks up the game's OWN arm64 libfmod.so out of the APK,
#     which every installed version ships. mcpelauncher then hooks FMOD::System::setOutput so
#     android FMOD drives the AAudio shim in fake_audio.cpp, which is backed by SDL3 and comes
#     out on CoreAudio. Sound keeps working.
#
#     TESTING NOTE, and it matters. The client is built with DEV_EXTRA_PATHS pointing at
#     $WS/mcpelauncher/mcpelauncher-mac-bin, and THAT tree also holds a libfmod.dylib.
#     findDataFile checks it before the bundle's own Resources dir, so on a build machine the
#     client still finds host FMOD after this removal and a naive "delete it and launch" test
#     passes for the wrong reason. Move that copy aside for the duration of the test:
#         mv "$WS/mcpelauncher/mcpelauncher-mac-bin/lib/native/arm64-v8a/libfmod.dylib"{,.off}
#     Then the log must read: FMOD "Failed to load host libfmod", immediately followed by the
#     linker loading versions/<v>/lib/arm64-v8a/libfmod.so and two "Found hook" lines for
#     FMOD::System::init and setOutput. That is the fallback actually engaging.
#     Proving sound is not silence: run with SDL_AUDIODRIVER=disk and
#     SDL_AUDIO_DISK_OUTPUT_FILE=/tmp/a.raw, then read /tmp/a.raw as signed 16-bit stereo.
#     Silence is all zeroes; a working menu gives roughly 99 percent non-zero samples.
#
#  2. The x86_64 clients. mcpelauncher-client and mcpelauncher-client32 are x86_64 and
#     nothing spawns them: bedrock.rs::direct_launch_target only ever builds the path
#     Contents/MacOS/mcpelauncher-client-<ANDROID_ABI>, which is arm64-v8a.
#
#  3. mcpelauncher-error. x86_64, links six Qt frameworks that the SLIM block above has
#     already removed, so it could not launch even if something asked for it. Only
#     mcpelauncher-ui-qt ever spawned it, and that is gone too.
#
#  4. The unversioned OpenSSL pair. Contents/Frameworks/lib{ssl,crypto}.dylib is a fat binary
#     whose i386 slice is OpenSSL 1.1.1x-dev: END OF LIFE since September 2023 and under the
#     old OpenSSL/SSLeay licence, not the Apache-2.0 that the 3.x pair carries and that our
#     NOTICE claims for all OpenSSL in the bundle. Nothing left in the bundle links it. The
#     only OpenSSL consumer is the arm64 client, which links @rpath/lib{ssl,crypto}.3.dylib
#     and carries a single rpath of @executable_path, so it resolves the 3.2.7 pair in
#     Contents/MacOS. Verified with otool -L over every Mach-O in MacOS/, Frameworks/ and
#     Frameworks/mvk-angle/, and by grepping every binary for a dlopen-by-name reference to
#     the unversioned files. There are none, and a launch test confirmed only the .3 pair
#     is mapped.
#     The DUPLICATE Contents/Frameworks/lib{ssl,crypto}.3.dylib is unreachable for the same
#     rpath reason, about 5 MB. It is Apache-2.0 and harmless, so it is deliberately LEFT in
#     place: no licence reason to touch it, so no reason to take the risk.
say "stripping: FMOD, the x86_64 clients, the Qt crash helper, end-of-life OpenSSL…"
rm -rf "$DST/Contents/Resources/mcpelauncher/lib/native"    # libfmod.dylib, both arches
rm -rf "$DST/Contents/Resources/mcpelauncher/lib/x86" "$DST/Contents/Resources/mcpelauncher/lib/x86_64"
rm -f  "$DST/Contents/MacOS/mcpelauncher-client" "$DST/Contents/MacOS/mcpelauncher-client32"
rm -f  "$DST/Contents/MacOS/mcpelauncher-error"
rm -f  "$DST/Contents/Frameworks/libssl.dylib" "$DST/Contents/Frameworks/libcrypto.dylib"

# If a future upstream bundle grows another FMOD copy somewhere else, fail loudly here rather
# than ship it. This is a licence guard, not a tidiness check.
if find "$DST" -name "libfmod*" | grep -q .; then
  echo "REFUSING TO PACKAGE: libfmod is still present in the bundle:"
  find "$DST" -name "libfmod*"
  echo "FMOD needs a paid commercial licence to ship in a product that sells anything."
  exit 1
fi

say "shipping nexus-webview as THE Xbox sign-in webview…"
NEXUS_WEBVIEW="$HOME/Downloads/nexus-antigravity/src-tauri/resources/nexus-webview"
[ -f "$NEXUS_WEBVIEW" ] || { echo "No nexus-webview at: $NEXUS_WEBVIEW (build it in the launcher repo)"; exit 1; }
cp -f "$NEXUS_WEBVIEW" "$DST/Contents/MacOS/mcpelauncher-webview"
chmod 755 "$DST/Contents/MacOS/mcpelauncher-webview"
rm -f "$DST/Contents/MacOS/mcpelauncher-webview.stock"   # slim runtime has no stock to revert to

say "shipping OpenSSL dylibs + relocating linkage to @rpath…"
CIB="$DST/Contents/MacOS/mcpelauncher-client-arm64-v8a"
if [ -d "$OPENSSL" ]; then
  cp -f "$OPENSSL/libssl.3.dylib" "$OPENSSL/libcrypto.3.dylib" "$DST/Contents/MacOS/"
  # dylib ids → @rpath, and libssl's libcrypto ref → @rpath
  install_name_tool -id @rpath/libssl.3.dylib    "$DST/Contents/MacOS/libssl.3.dylib"    2>/dev/null || true
  install_name_tool -id @rpath/libcrypto.3.dylib "$DST/Contents/MacOS/libcrypto.3.dylib" 2>/dev/null || true
  install_name_tool -change "$OPENSSL/libcrypto.3.dylib" @rpath/libcrypto.3.dylib "$DST/Contents/MacOS/libssl.3.dylib" 2>/dev/null || true
  # client's absolute OpenSSL refs → @rpath, and ensure it has an rpath to find them
  install_name_tool -change "$OPENSSL/libssl.3.dylib"    @rpath/libssl.3.dylib    "$CIB" 2>/dev/null || true
  install_name_tool -change "$OPENSSL/libcrypto.3.dylib" @rpath/libcrypto.3.dylib "$CIB" 2>/dev/null || true
  otool -l "$CIB" | grep -q "@executable_path" || install_name_tool -add_rpath @executable_path "$CIB" 2>/dev/null || true
fi

# -- LICENCE FILES IN THE BUNDLE ------------------------------------------------------------
# A GPL build has to carry its notice with it, not only next to the download. Generated from
# the one canonical template so the bundled copy can never drift from the published one.
# TAG is required: the notice names the release it belongs to, and an unstamped notice sends
# the reader looking for a source tag that does not exist.
NOTICE_TMPL="$(dirname "$0")/runtime-notice.txt"
GPL_TEXT="$(dirname "$0")/../LICENSE"
if [ -n "$TAG" ] && [ -f "$NOTICE_TMPL" ] && [ -f "$GPL_TEXT" ]; then
  say "writing the licence notice into the bundle (TAG=$TAG)…"
  sed "s|__RUNTIME_TAG__|${TAG}|" "$NOTICE_TMPL" > "$DST/Contents/Resources/NOTICE.txt"
  grep -q "__RUNTIME_TAG__" "$DST/Contents/Resources/NOTICE.txt" && { echo "tag substitution failed"; exit 1; }
  cp -f "$GPL_TEXT" "$DST/Contents/Resources/LICENSE-GPL-3.0.txt"
else
  echo "WARNING: no TAG set (or template missing) - the bundle will carry a STALE licence notice."
  echo "         Re-run as: TAG=v1.7.6-572-nexusN $0"
fi

say "re-signing (ad-hoc)…"
/usr/bin/xattr -cr "$DST"
codesign --force --deep --sign - "$DST" 2>&1 | tail -1

# -- DMG: the artefact publish-runtime-patched.sh actually uploads --------------------------
# This step used to be done by hand, which is how a hand-stripped bundle and a script-built
# one drifted apart. Building it here means the STRIP block above is binding on what ships.
DMG_OUT="$HOME/Downloads/nexus-antigravity/runtime-release/Nexus-Bedrock-Runtime.dmg"

# HUD REGRESSION GUARD. Writing the DMG replaces the artefact publish-runtime-patched.sh uploads.
# The build workspace currently reproduces nexus6, which has none of the 16 nexus_* HUD settings the
# shipped client carries, so a plain "rebuild then package" silently produces a release candidate
# with the whole Nexus HUD deleted. Compare the client we just swapped in against the installed one
# and refuse the DMG if it went backwards. The bundle itself is still built, so the launch test in
# REBUILD.md step 7 works either way.
NEW_HUD=$(strings -a "$DST/Contents/MacOS/mcpelauncher-client-arm64-v8a" | grep -c '^nexus_' || true)
OLD_HUD=$(strings -a "$SRC/Contents/MacOS/mcpelauncher-client-arm64-v8a" | grep -c '^nexus_' || true)
if [ "$NEW_HUD" -lt "$OLD_HUD" ] && [ "${ALLOW_HUD_REGRESSION:-0}" != "1" ]; then
  cat >&2 <<MSG

REFUSING TO BUILD THE DMG.

The client just built carries $NEW_HUD nexus_* settings; the installed runtime carries $OLD_HUD.
This build is missing HUD features that players are using today, so writing
  $DMG_OUT
would replace the release candidate with a runtime that has no Nexus HUD.

The bundle is still at $DST if you only wanted a local launch test.
If you really mean to ship a build with fewer features, re-run with ALLOW_HUD_REGRESSION=1.
MSG
  echo "PACKAGE_DONE (no DMG)  ->  $DST"
  exit 0
fi

if [ -d "$(dirname "$DMG_OUT")" ]; then
  say "building the DMG…"
  rm -f "$DMG_OUT"
  hdiutil create -volname "Minecraft Bedrock" -srcfolder "$WS/patched-runtime" \
    -ov -format UDZO "$DMG_OUT" >/dev/null
  say "DMG: $DMG_OUT ($(stat -f%z "$DMG_OUT") bytes)"
else
  say "skipping the DMG, no runtime-release dir at $(dirname "$DMG_OUT")"
fi

echo "PACKAGE_DONE  ->  $DST"
