#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
Usage:
  scripts/observe/strace_clean.sh [--dry-run|-n]

Description:
  Empty artifacts/strace (remove its contents) but keep the directory itself.

Options:
  -n, --dry-run   Print what would be removed, without deleting.
USAGE
}

dry_run=0
if [[ "${1-}" == "-n" || "${1-}" == "--dry-run" ]]; then
  dry_run=1
  shift
fi

if [[ $# -ne 0 ]]; then
  usage
  exit 2
fi

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"

target="$repo_root/artifacts/strace"

# Resolve real paths for safety
repo_real="$(readlink -f -- "$repo_root")"
target_real="$(readlink -f -- "$target" 2>/dev/null || true)"

if [[ -z "$target_real" ]]; then
  # artifacts/strace が存在しない場合は何もしない
  echo "Nothing to do: $target (missing)"
  exit 0
fi

# Strong safety check: only allow exactly <repo>/artifacts/strace
if [[ "$target_real" != "$repo_real/artifacts/strace" ]]; then
  echo "Refusing to operate on unexpected path:" >&2
  echo "  repo  : $repo_real" >&2
  echo "  target: $target_real" >&2
  exit 1
fi

if [[ ! -d "$target_real" ]]; then
  echo "Nothing to do: $target_real (not a directory)"
  exit 0
fi

# List items to delete (top-level children only)
mapfile -d '' items < <(find "$target_real" -mindepth 1 -maxdepth 1 -print0)

if (( ${#items[@]} == 0 )); then
  echo "Already clean: $target_real"
  exit 0
fi

if (( dry_run )); then
  echo "[dry-run] would remove:"
  printf '  %s\n' "${items[@]}"
  exit 0
fi

# Delete contents
find "$target_real" -mindepth 1 -maxdepth 1 -exec rm -rf -- {} +

echo "Cleaned: $target_real"
