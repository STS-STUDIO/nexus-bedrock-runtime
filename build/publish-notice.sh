#!/bin/bash
# =============================================================================
# Nexus — republish ONLY the GPL notice on the runtime download channel.
#
# The notice served next to Nexus-Bedrock-Runtime.dmg went stale: it claimed the
# runtime was an unmodified upstream build and pointed at github.com/minecraft-linux
# instead of our source mirror. Both publish scripts now emit the correct text,
# but they only run when a new DMG goes out, so the wrong file stays live until
# the next release. This uploads the notice on its own — no DMG, no manifest, no
# re-download for any player.
#
#   NEXUS_RELEASE_SECRET="<secret>" ./publish-notice.sh
#
# TAG defaults to whatever the live runtime-manifest.json says is shipping, which
# is what you want: the notice has to name the build people actually have.
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"

SITE="${SITE:-https://stsstudio.org}"
[ -n "${NEXUS_RELEASE_SECRET:-}" ] || { echo "Set NEXUS_RELEASE_SECRET first."; exit 1; }
[ -f runtime-notice.txt ] || { echo "Missing runtime-notice.txt."; exit 1; }

if [ -z "${TAG:-}" ]; then
  echo "Reading the live runtime tag from $SITE …"
  TAG=$(curl -sS --fail --http1.1 --retry 3 --connect-timeout 20 \
        "$SITE/nexus/download/runtime-manifest.json" \
        | python3 -c "import json,sys;print(json.load(sys.stdin)['tag'])")
  [ -n "$TAG" ] || { echo "Could not read the live tag; pass TAG= explicitly."; exit 1; }
fi
echo "Live runtime tag: $TAG"

mkdir -p runtime-release
sed "s|__RUNTIME_TAG__|${TAG}|" runtime-notice.txt > runtime-release/NOTICE.txt
grep -q "__RUNTIME_TAG__" runtime-release/NOTICE.txt && { echo "Tag substitution failed."; exit 1; }

echo "--- NOTICE.txt to be published ($(wc -l < runtime-release/NOTICE.txt) lines) ---"
head -5 runtime-release/NOTICE.txt
echo "---"

curl -sS --fail --http1.1 --retry 5 --retry-all-errors \
  -X POST "$SITE/nexus/release/upload?name=NOTICE.txt" \
  -H "Authorization: Bearer $NEXUS_RELEASE_SECRET" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@runtime-release/NOTICE.txt" > /dev/null

echo "Verifying what the channel now serves …"
served=$(curl -sS --fail --http1.1 --retry 3 "$SITE/nexus/download/NOTICE.txt")
if diff -q <(echo "$served") runtime-release/NOTICE.txt > /dev/null; then
  echo "Published. The served notice matches the file, tag $TAG."
else
  echo "Uploaded, but the served copy differs (CDN cache?). Re-check in a minute."
  exit 1
fi
