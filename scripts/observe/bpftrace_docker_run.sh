#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || "$1" != "--" ]]; then
  echo "Usage: $0 -- <command> [args...]" >&2
  exit 2
fi
shift # drop --

if ! command -v docker >/dev/null 2>&1; then
  echo "docker not found in PATH" >&2
  exit 1
fi

image="${BPFTRACE_DOCKER_IMAGE:-quay.io/iovisor/bpftrace:latest}"
root_dir="$(pwd)"

cmd_quoted=()
for arg in "$@"; do
  printf -v q '%q' "$arg"
  cmd_quoted+=("$q")
done

cmd_line="BPFTRACE_TMP_ROOT=/tmp ./scripts/observe/bpftrace_run.sh -- ${cmd_quoted[*]}"

exec docker run --rm --privileged --pid=host \
  -v /sys:/sys \
  -v /lib/modules:/lib/modules \
  -v /usr/src:/usr/src \
  -v /etc/os-release:/etc/os-release:ro \
  -v "$root_dir":/work \
  -w /work \
  "$image" \
  /bin/bash -lc "$cmd_line"
