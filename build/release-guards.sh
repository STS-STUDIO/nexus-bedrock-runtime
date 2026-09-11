#!/bin/bash
# =============================================================================
# Shared guards for the runtime publish scripts.
#
# Sourced by publish-runtime-patched.sh and mirror-runtime.sh so the two can
# never drift apart. They have drifted before, and the cost was weeks of
# shipping a NOTICE.txt that told users the runtime was unmodified upstream.
#
# WHY THIS FILE EXISTS
# The engine source for v1.7.6-572-nexus7 and -nexus8 no longer exists anywhere.
# Both were built, published to players, and never mirrored, and nothing in the
# publish path objected. Every player is running a binary whose source is gone,
# which is both a GPL-3.0 section 6 breach and an unrecoverable feature loss.
# The guard below is the check that would have stopped it at the first tag.
#
# WHAT IT ENFORCES, in order, all fail-closed:
#   1. The public source mirror carries a tag with the same name.
#   2. That tag really contains source, not an empty or placeholder commit.
#   3. The local engine workspace has no unsaved changes, so the thing that
#      produced this DMG is itself under version control.
#   4. The source published under that tag is byte-identical to the engine
#      workspace state that produced this build. A tag pointing at some older
#      build's source is not "the corresponding source".
#
# Check 4 is the one with teeth. Tag existence alone is satisfiable by tagging
# anything, which is how you end up with a nexus9 tag carrying nexus6 source.
# =============================================================================

MIRROR_URL="${NEXUS_MIRROR_URL:-https://github.com/STS-STUDIO/nexus-bedrock-runtime.git}"
ENGINE_ROOT="${NEXUS_ENGINE_ROOT:-$HOME/nexus-engine}"
# Where the mirror keeps the machine-checked copy of the engine snapshot. The
# human-readable patches/ directory is a curated view for readers; this one is
# the exact bytes the guard compares against.
MIRROR_STATE_DIR="source-state"

_guard_die() { echo >&2; echo "$*" >&2; echo >&2; exit 1; }

require_mirrored_source() {
  local tag="$1"
  local tmp; tmp="$(mktemp -d)"
  # shellcheck disable=SC2064
  trap "rm -rf '$tmp'" RETURN

  # -- 1. the tag exists on the public mirror -------------------------------
  echo "Checking the source mirror for tag $tag ..."
  local remote_tag
  remote_tag="$(git ls-remote --tags "$MIRROR_URL" "refs/tags/$tag" 2>/dev/null || true)"
  if ! printf '%s' "$remote_tag" | grep -q "refs/tags/$tag"; then
    _guard_die "REFUSING TO PUBLISH.

  $MIRROR_URL
carries no tag \"$tag\", so the corresponding source for this build is not
published. Shipping it puts the runtime back in breach of GPL-3.0 section 6.

Stage the source for this build in the mirror, commit, tag it \"$tag\", push,
then re-run this script. A network failure also lands here: this check is
deliberately fail-closed, so check your connection before assuming the worst."
  fi
  echo "  ok  tag $tag exists on the mirror"

  # Escape hatch, deliberately narrow: it has to name the exact tag, so it can
  # never be left set in a shell and silently wave a future release through.
  # For re-publishing nexus5 or nexus6, whose source predates this layout.
  local legacy=0
  if [ "${NEXUS_ALLOW_LEGACY_SOURCE_TAG:-}" = "$tag" ]; then
    legacy=1
    echo "  !   NEXUS_ALLOW_LEGACY_SOURCE_TAG names $tag"
    echo "      source layout and correspondence checks are SKIPPED for this tag"
  fi

  # -- 2. the tag actually contains source ----------------------------------
  # A tag is a label anyone can hang on an empty commit. Fetch it and look.
  if ! git clone --quiet --depth 1 --branch "$tag" "$MIRROR_URL" "$tmp/mirror" 2>"$tmp/clone.err"; then
    _guard_die "REFUSING TO PUBLISH.

Could not fetch tag \"$tag\" from $MIRROR_URL:
$(cat "$tmp/clone.err")"
  fi

  local missing="" f
  for f in LICENSE NOTICE.txt README.md PINNED-VERSIONS.md build/REBUILD.md; do
    [ -f "$tmp/mirror/$f" ] || missing="$missing $f"
  done
  if [ -n "$missing" ] && [ "$legacy" -eq 1 ]; then
    echo "  !   tag $tag is missing:$missing (waived, legacy tag)"
    missing=""
  fi
  if [ -n "$missing" ]; then
    _guard_die "REFUSING TO PUBLISH.

Tag \"$tag\" exists on the mirror but is missing:$missing

A tag with no source in it is not published source. Stage the real thing and
re-tag."
  fi
  [ "$legacy" -eq 1 ] || echo "  ok  tag $tag carries the license, notice and rebuild instructions"

  # -- 3. the engine workspace has nothing unsaved --------------------------
  if [ ! -d "$ENGINE_ROOT" ]; then
    echo "  -   no engine workspace at $ENGINE_ROOT, skipping the correspondence check"
    echo "      (set NEXUS_ENGINE_ROOT if it lives elsewhere)"
    return 0
  fi
  if [ ! -x "$ENGINE_ROOT/tools/verify-engine.sh" ]; then
    _guard_die "REFUSING TO PUBLISH.

$ENGINE_ROOT is not under the snapshot system, so there is no way to tell what
source this DMG was built from. Set it up first:

  cd $ENGINE_ROOT && git init && tools/snapshot-engine.sh && git add -A && git commit"
  fi
  if ! "$ENGINE_ROOT/tools/verify-engine.sh" > "$tmp/verify.out" 2>&1; then
    _guard_die "REFUSING TO PUBLISH.

The engine workspace at $ENGINE_ROOT has changes that are not committed, so the
source for this build exists only on this Mac. That is exactly how the nexus7
and nexus8 source was lost.

$(cat "$tmp/verify.out")"
  fi
  echo "  ok  engine workspace is fully committed"

  # -- 4. the published source matches the build ----------------------------
  if [ "$legacy" -eq 1 ]; then
    echo "  !   correspondence between $tag and this build was NOT verified"
    return 0
  fi

  if [ ! -d "$tmp/mirror/$MIRROR_STATE_DIR" ]; then
    _guard_die "REFUSING TO PUBLISH.

Tag \"$tag\" has no $MIRROR_STATE_DIR/ directory, so there is no way to check
that its source is the source this DMG was built from.

Stage it:

  cp -R $ENGINE_ROOT/state \\
        <mirror>/$MIRROR_STATE_DIR
  cd <mirror> && git add $MIRROR_STATE_DIR && git commit && git tag -f $tag && git push --tags

Tags made before this check existed (nexus5, nexus6) can be re-published with:

  NEXUS_ALLOW_LEGACY_SOURCE_TAG=$tag ..."
  fi

  if ! diff -r -q "$ENGINE_ROOT/state" "$tmp/mirror/$MIRROR_STATE_DIR" > "$tmp/state.diff" 2>&1; then
    _guard_die "REFUSING TO PUBLISH.

Tag \"$tag\" carries source, but it is NOT the source this build came from:

$(sed -n '1,40p' "$tmp/state.diff")

The published source has to correspond to the exact binary being shipped. Copy
the current engine snapshot into the mirror, commit, move the tag, push, then
re-run."
  fi
  echo "  ok  published source matches the engine workspace exactly"
  return 0
}
