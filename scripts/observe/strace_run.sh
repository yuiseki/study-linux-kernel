#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || "$1" != "--" ]]; then
  echo "Usage: $0 -- <command> [args...]" >&2
  exit 2
fi
shift # drop --

cmd="$1"
shift || true

ts="$(date +%Y%m%d-%H%M%S)"
safe_name="$(basename "$cmd" | tr -cd 'A-Za-z0-9._-')"
outdir="artifacts/strace/${safe_name}-${ts}"
mkdir -p "$outdir"

# トレース対象コマンドの出力は outdir に保存し、stdout は outdir の1行だけにする
strace -ff -tt -T -s 256 -yy \
  -o "$outdir/trace" \
  "$cmd" "$@" \
  >"$outdir/stdout.txt" \
  2>"$outdir/stderr.txt"

printf '%s\n' "$outdir"
