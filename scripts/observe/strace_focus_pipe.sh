#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

if [[ $# -ne 1 ]]; then
  echo "Usage: $0 <strace_outdir>" >&2
  exit 2
fi

dir="$1"
out="$dir/focus_pipe.txt"
files=("$dir"/trace.*)
: > "$out"

if (( ${#files[@]} == 0 )); then
  echo "No trace files found under: $dir" >&2
  echo "$out"
  exit 0
fi

grep -h -n -E \
  ' (execve|clone|fork|vfork|wait4|waitid|exit_group)\('\
'| (pipe2?|dup2?|openat|close|fcntl)\('\
'| (read|write|readv|writev)\(' \
  "${files[@]}" \
  > "$out" || true

# loader/lib/locale ノイズも落とす
grep -v -E \
  '/etc/ld\.so\.cache|/lib/|/usr/lib/|/usr/share/locale|locale-archive' \
  "$out" > "$out.tmp" || true
mv "$out.tmp" "$out"

echo "$out"
