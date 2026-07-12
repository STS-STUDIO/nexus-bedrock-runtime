#!/bin/bash
# =============================================================================
# Nexus — publish the PATCHED Bedrock runtime (our own native-arm64 engine build
# with the JNI-thread crash fix + OpenGL ES 3.1 fix).
#
# Unlike mirror-runtime.sh, this does NOT re-download upstream — it publishes the
# DMG we built from ~/nexus-engine/patched-runtime, under a bumped tag so every
# client re-downloads it. Host defaults to Railway (always reachable; stsstudio.org
# 403s behind Cloudflare). Override SITE/TAG via env.
#
#   NEXUS_RELEASE_SECRET="<secret>" ./publish-runtime-patched.sh
#   NEXUS_RELEASE_SECRET="<secret>" SITE="https://stsstudio.org" TAG="v1.7.6-572-nexus5" ./publish-runtime-patched.sh
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"

SITE="${SITE:-https://stsstudio-production.up.railway.app}"
TAG="${TAG:-v1.7.6-572-nexus5}"
DMG="runtime-release/Nexus-Bedrock-Runtime.dmg"
[ -n "${NEXUS_RELEASE_SECRET:-}" ] || { echo "Set NEXUS_RELEASE_SECRET first."; exit 1; }
[ -f "$DMG" ] || { echo "Missing $DMG — build it first (see docs/engine-build-plan.md)."; exit 1; }

SIZE=$(stat -f%z "$DMG")
echo "Publishing patched runtime: tag=$TAG size=$SIZE → $SITE"

cat > runtime-release/runtime-manifest.json <<EOF
{
  "tag": "${TAG}",
  "dmg_url": "${SITE}/nexus/download/Nexus-Bedrock-Runtime.dmg",
  "dmg_name": "Nexus-Bedrock-Runtime.dmg",
  "size": ${SIZE}
}
EOF

cat > runtime-release/NOTICE.txt <<'EOF'
Nexus Bedrock Runtime — third-party license notice

This Bedrock runtime is a build of the mcpelauncher project (GNU GPL-3.0), with
small compatibility patches by STS Studio. Corresponding source + patches:
https://github.com/minecraft-linux  and the Nexus engine-patches.
License text: https://www.gnu.org/licenses/gpl-3.0.txt

The game itself is never distributed here; it is downloaded from each user's own
Google Play account. Nexus Launcher (proprietary, by STS Studio) runs this runtime
as an independent process.
EOF

upload_small() {
  echo "  uploading $1 …"
  curl -sS --fail --http1.1 --retry 5 --retry-all-errors -X POST "$SITE/nexus/release/upload?name=$1" \
    -H "Authorization: Bearer $NEXUS_RELEASE_SECRET" -H "Content-Type: application/octet-stream" \
    --data-binary "@runtime-release/$1" > /dev/null
}
upload_big() { # 25MB pieces, reassembled server-side (under Cloudflare's request cap)
  local name="$1" src="runtime-release/$1"; local dir; dir=$(mktemp -d)
  split -b 25m "$src" "$dir/piece_"
  local mode="start" out=""
  for piece in "$dir"/piece_*; do
    echo "  uploading $name ($(basename "$piece"), mode=$mode) …"
    out=$(curl -sS --fail --http1.1 --retry 4 --retry-all-errors --connect-timeout 20 --max-time 600 \
      -X POST "$SITE/nexus/release/upload?name=$name&mode=$mode" \
      -H "Authorization: Bearer $NEXUS_RELEASE_SECRET" -H "Content-Type: application/octet-stream" \
      --data-binary "@$piece"); mode="append"
  done
  rm -rf "$dir"
  local got; got=$(echo "$out" | python3 -c "import json,sys;print(json.load(sys.stdin)['size'])" 2>/dev/null || echo "?")
  [ "$got" = "$SIZE" ] || { echo "SIZE MISMATCH after upload ($got != $SIZE) — aborting"; exit 1; }
  echo "  ✔ $name complete ($got bytes)"
}

echo "Publishing to ${SITE} …"
upload_big   "Nexus-Bedrock-Runtime.dmg"
upload_small "NOTICE.txt"
upload_small "runtime-manifest.json"   # last, so it never points at a missing file
echo "✅ Patched runtime published (tag=$TAG). Clients on the new Nexus will re-download it."
