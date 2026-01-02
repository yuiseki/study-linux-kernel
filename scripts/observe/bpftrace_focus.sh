#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
Usage:
  bpftrace_focus.sh <bpftrace_outdir> [--keep-noise]

Default:
  - Reads trace.txt and writes focus.txt with loader/lib/locale noise removed

Options:
  --keep-noise   Do not drop loader/lib/locale noise.
USAGE
}

if [[ $# -lt 1 || $# -gt 2 ]]; then
  usage
  exit 2
fi

dir="$1"
keep_noise="${2:-}"

in="$dir/trace.txt"
out="$dir/focus.txt"

if [[ ! -f "$in" ]]; then
  echo "No trace.txt found under: $dir" >&2
  echo "$out"
  exit 0
fi

: > "$out"

if [[ "$keep_noise" != "--keep-noise" ]]; then
  grep -v -E \
    'file=/etc/ld\.so\.cache|file=/lib/|file=/usr/lib/|file=/etc/nsswitch\.conf|file=/etc/passwd|file=/usr/share/locale|file=locale-archive' \
    "$in" \
    > "$out" || true
else
  cp "$in" "$out"
fi

echo "$out"
