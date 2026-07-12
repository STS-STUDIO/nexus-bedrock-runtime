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
