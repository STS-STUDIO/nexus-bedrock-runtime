#!/bin/bash
# =============================================================================
# Nexus — package the PATCHED Bedrock runtime bundle.
# Reconstructed 2026-07-11 (workspace was deleted). Swaps our freshly-built patched
# mcpelauncher-client into a copy of the installed runtime bundle, ships the OpenSSL
# dylibs, fixes @rpath, and re-signs. Output: ~/nexus-engine/patched-runtime.
# Then publish with ~/Downloads/nexus-antigravity/publish-runtime-patched.sh (bump TAG).
# =============================================================================
set -euo pipefail
say() { echo "== $*"; }

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
# (replaced by nexus-webview below). msa-daemon and the x86_64 clients are KEPT this tag —
# they're small-ish and unproven-safe to drop; candidates for a later wave.
say "slimming: removing Qt frameworks, plugins, qml, ui-qt, msa-ui-qt…"
rm -rf "$DST/Contents/Frameworks/"Qt*.framework
rm -rf "$DST/Contents/Frameworks/Sparkle.framework"
rm -rf "$DST/Contents/Frameworks/__MACOSX"
rm -rf "$DST/Contents/PlugIns"
rm -rf "$DST/Contents/Resources/qml"
rm -f  "$DST/Contents/Resources/qt.conf"
rm -f  "$DST/Contents/MacOS/mcpelauncher-ui-qt"
rm -f  "$DST/Contents/MacOS/msa-ui-qt"

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

say "re-signing (ad-hoc)…"
/usr/bin/xattr -cr "$DST"
codesign --force --deep --sign - "$DST" 2>&1 | tail -1

echo "PACKAGE_DONE  →  $DST"
