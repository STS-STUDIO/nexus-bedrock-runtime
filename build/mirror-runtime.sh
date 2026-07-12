#!/bin/bash
# =============================================================================
# Nexus — Bedrock runtime mirror (own-domain, no third-party pages/repos).
#
# Downloads the latest open-source runtime build, renames it to Nexus branding,
# and publishes it to stsstudio.org. The big DMG is uploaded in 50MB pieces
# because Cloudflare caps a single request at ~100MB. Run occasionally (e.g.
# monthly) to pick up upstream fixes:
#
#   NEXUS_RELEASE_SECRET="<your release secret>" ./mirror-runtime.sh
#
# NOTICE.txt is published alongside it — the runtime is GPL-3.0 open source, and
# keeping a small license/source notice on the download channel is what makes
# redistributing it legal. It's a plain text file; it's not shown on any page.
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"

SITE="https://stsstudio.org"
[ -n "${NEXUS_RELEASE_SECRET:-}" ] || { echo "Set NEXUS_RELEASE_SECRET first."; exit 1; }

# Every network call: HTTP/1.1 (avoids flaky h2 stream resets) + retries.
CURL="curl -sSL --fail --http1.1 --retry 5 --retry-all-errors --connect-timeout 20"

echo "Finding the latest runtime build…"
api="https://api.github.com/repos/minecraft-linux/macos-builder/releases?per_page=10"
json=$($CURL -H "Accept: application/vnd.github+json" "$api")
sel=$(echo "$json" | python3 -c "
import json,sys
for r in json.load(sys.stdin):
    if r.get('prerelease'): continue
    for a in r.get('assets', []):
        n = a['name']
        if 'macOS' in n and n.endswith('.dmg') and 'x86_64-0.2.' in n:
            print(r['tag_name'], a['browser_download_url'], a['size'], sep='\t'); raise SystemExit
")
TAG=$(echo "$sel" | cut -f1); URL=$(echo "$sel" | cut -f2); SIZE=$(echo "$sel" | cut -f3)
[ -n "$TAG" ] || { echo "Could not find an upstream build."; exit 1; }
echo "  found $TAG ($SIZE bytes)"

mkdir -p runtime-release
echo "Downloading…"
# --retry + -C - : resume/retry on transient network errors; skip if already
# fully downloaded from a previous run.
if [ ! -f "runtime-release/Nexus-Bedrock-Runtime.dmg" ] || [ "$(stat -f%z runtime-release/Nexus-Bedrock-Runtime.dmg)" != "$SIZE" ]; then
  $CURL -C - -o "runtime-release/Nexus-Bedrock-Runtime.dmg" "$URL"
fi
[ "$(stat -f%z runtime-release/Nexus-Bedrock-Runtime.dmg)" = "$SIZE" ] || { echo "Download incomplete — rerun."; exit 1; }

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

The Bedrock runtime distributed at this URL is an unmodified build of the
mcpelauncher project, free software licensed under the GNU General Public
License v3.0. Corresponding source code: https://github.com/minecraft-linux
License text: https://www.gnu.org/licenses/gpl-3.0.txt

The game itself is never distributed here; it is downloaded from each user's
own Google Play account. Nexus Launcher (a separate, proprietary application
by STS Studio) runs this runtime as an independent process.
EOF

# --fail makes curl exit non-zero on any HTTP error, which aborts the script
# (set -e) — so a failed upload can never leave a manifest pointing at nothing.
upload_small() {
  echo "  uploading $1 …"
  curl -sS --fail -X POST "$SITE/nexus/release/upload?name=$1" \
    -H "Authorization: Bearer $NEXUS_RELEASE_SECRET" \
    -H "Content-Type: application/octet-stream" \
    --data-binary "@runtime-release/$1" > /dev/null
}

upload_big() { # 25MB pieces: under Cloudflare's request cap, reassembled server-side.
  # --http1.1 avoids HTTP/2 stream resets on big bodies through Cloudflare, and
  # each piece retries on any error (a failed piece never half-appends: the
  # server only writes fully-received bodies, so retrying the same piece is safe).
  local name="$1" src="runtime-release/$1"
  local dir; dir=$(mktemp -d)
  split -b 25m "$src" "$dir/piece_"
  local mode="start" out=""
  for piece in "$dir"/piece_*; do
    echo "  uploading $name ($(basename "$piece"), mode=$mode) …"
    out=$(curl -sS --fail --http1.1 --retry 4 --retry-all-errors --connect-timeout 20 --max-time 600 \
      -X POST "$SITE/nexus/release/upload?name=$name&mode=$mode" \
      -H "Authorization: Bearer $NEXUS_RELEASE_SECRET" \
      -H "Content-Type: application/octet-stream" \
      --data-binary "@$piece")
    mode="append"
  done
  rm -rf "$dir"
  local expect got
  expect=$(stat -f%z "$src")
  got=$(echo "$out" | python3 -c "import json,sys;print(json.load(sys.stdin)['size'])")
  [ "$expect" = "$got" ] || { echo "SIZE MISMATCH after upload ($got != $expect) — aborting"; exit 1; }
  echo "  ✔ $name complete ($got bytes)"
}

echo "Publishing to ${SITE} …"
upload_big   "Nexus-Bedrock-Runtime.dmg"
upload_small "NOTICE.txt"
upload_small "runtime-manifest.json"   # last, so it never points at a missing file
echo "✅ Runtime published. The launcher now installs Bedrock from ${SITE}."
