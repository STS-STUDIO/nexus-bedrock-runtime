#!/bin/bash
# =============================================================================
# Nexus — publish the PATCHED Bedrock runtime (our own native-arm64 engine build
# with the JNI-thread crash fix + OpenGL ES 3.1 fix).
#
# Unlike mirror-runtime.sh, this does NOT re-download upstream — it publishes the
# DMG we built from ~/nexus-engine/patched-runtime, under a bumped tag so every
# client re-downloads it. Host is the Hostinger VPS (stsstudio.org). ALWAYS pass
# TAG explicitly, bumped past the live one (check the served runtime-manifest.json).
#
#   NEXUS_RELEASE_SECRET="<secret>" TAG="v1.7.6-572-nexus9" ./publish-runtime-patched.sh
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"

SITE="${SITE:-https://stsstudio.org}"
TAG="${TAG:?set TAG explicitly (e.g. v1.7.6-572-nexus9) — check the live runtime-manifest.json first}"
DMG="runtime-release/Nexus-Bedrock-Runtime.dmg"
[ -n "${NEXUS_RELEASE_SECRET:-}" ] || { echo "Set NEXUS_RELEASE_SECRET first."; exit 1; }
[ -f "$DMG" ] || { echo "Missing $DMG — build it first (see docs/engine-build-plan.md)."; exit 1; }

# The GPL notice that ships next to the DMG. One template, one substitution, so
# the two publish scripts can never drift apart again (they did, and one of them
# spent weeks telling users the runtime was unmodified upstream).
write_notice() {
  local tag="$1" tmpl="runtime-notice.txt"
  [ -f "$tmpl" ] || { echo "Missing $tmpl — cannot publish without the GPL notice."; exit 1; }
  sed "s|__RUNTIME_TAG__|${tag}|" "$tmpl" > runtime-release/NOTICE.txt
  grep -q "__RUNTIME_TAG__" runtime-release/NOTICE.txt && { echo "NOTICE.txt tag substitution failed."; exit 1; }
  return 0
}

# GPL-3.0 section 6: the corresponding source has to be available for the exact
# build being shipped. Refuse to publish a tag the source mirror does not carry.
require_mirrored_source() {
  local tag="$1" mirror="https://github.com/STS-STUDIO/nexus-bedrock-runtime.git"
  echo "Checking the source mirror for tag $tag …"
  if git ls-remote --tags "$mirror" "refs/tags/$tag" 2>/dev/null | grep -q "refs/tags/$tag"; then
    echo "  ✔ source published for $tag"
    return 0
  fi
  cat >&2 <<MSG

REFUSING TO PUBLISH.

  $mirror
carries no tag "$tag", so the corresponding source for this build is not
published. Shipping it puts the runtime back in breach of GPL-3.0 section 6.

Stage the patches for this build in ~/Desktop/nexus-bedrock-runtime, commit,
tag it "$tag", push, then re-run this script.
MSG
  exit 1
}

require_mirrored_source "$TAG"

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

write_notice "$TAG"

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
