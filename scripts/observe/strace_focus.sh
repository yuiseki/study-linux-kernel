#!/usr/bin/env bash
set -euo pipefail
shopt -s nullglob

usage() {
  cat >&2 <<'USAGE'
Usage:
  strace_focus.sh <strace_outdir> [--keep-noise]

Default:
  - Extracts "shell-relevant" syscalls from trace.* into focus.txt
  - Drops common loader/libc/locale noise to keep the log human-readable

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

out="$dir/focus.txt"
tmp="$dir/focus.tmp"

files=("$dir"/trace.*)
: > "$out"

if (( ${#files[@]} == 0 )); then
  echo "No trace files found under: $dir" >&2
  echo "$out"
  exit 0
fi

# まず “シェル理解に直結する syscalls” を拾う
grep -h -n -E \
  ' (execve|clone|fork|vfork|wait4|waitid|exit_group)\('\
'| (pipe2?|dup2?|openat|close|fcntl)\('\
'| (read|write|pread64|pwrite64|readv|writev)\('\
'| (chdir|getcwd)\('\
'| (setpgid|setsid|tcsetpgrp|ioctl)\('\
'| (sigaction|rt_sigaction|rt_sigprocmask|kill)\(' \
  "${files[@]}" \
  > "$tmp" || true

# ノイズを落とす（必要なら --keep-noise で無効化）
if [[ "$keep_noise" != "--keep-noise" ]]; then
  # 典型ノイズ:
  # - 動的リンカ/共有ライブラリ/ロケール
  # - これらは pipe/redirect の理解には基本不要
  grep -v -E \
    '/etc/ld\.so\.cache|/lib/|/usr/lib/|/usr/share/locale|locale-archive' \
    "$tmp" \
    > "$out" || true
else
  mv "$tmp" "$out"
fi

rm -f "$tmp"
echo "$out"
