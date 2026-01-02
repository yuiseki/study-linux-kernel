#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 || "$1" != "--" ]]; then
  echo "Usage: $0 -- <command> [args...]" >&2
  exit 2
fi
shift # drop --

cmd="$1"
shift || true

if ! command -v bpftrace >/dev/null 2>&1; then
  echo "bpftrace not found in PATH" >&2
  exit 1
fi
if [[ "$(id -u)" != "0" ]]; then
  echo "bpftrace requires root privileges (run with sudo)" >&2
  exit 1
fi

ts="$(date +%Y%m%d-%H%M%S)"
safe_name="$(basename "$cmd" | tr -cd 'A-Za-z0-9._-')"
root_dir="$(pwd -P)"
outdir="${root_dir}/artifacts/bpftrace/${safe_name}-${ts}"
mkdir -p "$outdir"
tmp_root="${BPFTRACE_TMP_ROOT:-${root_dir}/tmp}"
mkdir -p "$tmp_root"

cmd_quoted=()
for arg in "$cmd" "$@"; do
  printf -v q '%q' "$arg"
  cmd_quoted+=("$q")
done

cat >"$outdir/run.sh" <<EOF_RUN
#!/usr/bin/env bash
set -euo pipefail
cmd=(${cmd_quoted[*]})
exec "\${cmd[@]}" >"$outdir/stdout.txt" 2>"$outdir/stderr.txt"
EOF_RUN
chmod +x "$outdir/run.sh"

tmp_dir="$(mktemp -d "${tmp_root}/bpftrace-run.XXXXXX")"
tmp_run="${tmp_dir}/run.sh"
cp "$outdir/run.sh" "$tmp_run"
chmod +x "$tmp_run"

cleanup() {
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

launcher="/usr/bin/env"
if [[ ! -x "$launcher" ]]; then
  echo "launcher not found or not executable: $launcher" >&2
  exit 1
fi
cmd_for_bpftrace="$launcher $tmp_run"

cat >"$outdir/trace.bt" <<'EOF_BT'
BEGIN
{
  @trace[cpid] = 1;
}

tracepoint:sched:sched_process_fork
/@trace[args->parent_pid]/
{
  @trace[args->child_pid] = 1;
}

tracepoint:sched:sched_process_exit
/@trace[pid]/
{
  delete(@trace[pid]);
}

tracepoint:syscalls:sys_enter_openat
/@trace[pid]/
{
  printf("%-16s pid=%d openat dirfd=%d file=%s flags=0x%x\n",
         comm, pid, args->dfd, str(args->filename), args->flags);
}

tracepoint:syscalls:sys_enter_read
/@trace[pid]/
{
  printf("%-16s pid=%d read  fd=%d count=%d\n",
         comm, pid, args->fd, args->count);
}

tracepoint:syscalls:sys_enter_write
/@trace[pid]/
{
  printf("%-16s pid=%d write fd=%d count=%d\n",
         comm, pid, args->fd, args->count);
}

tracepoint:syscalls:sys_enter_connect
/@trace[pid]/
{
  printf("%-16s pid=%d connect fd=%d addrlen=%d\n",
         comm, pid, args->fd, args->addrlen);
}

tracepoint:syscalls:sys_enter_accept
/@trace[pid]/
{
  printf("%-16s pid=%d accept fd=%d\n",
         comm, pid, args->fd);
}

tracepoint:syscalls:sys_enter_accept4
/@trace[pid]/
{
  printf("%-16s pid=%d accept4 fd=%d\n",
         comm, pid, args->fd);
}
EOF_BT

bpftrace -q -o "$outdir/trace.txt" "$outdir/trace.bt" -c "$cmd_for_bpftrace" \
  >"$outdir/bpftrace_stdout.txt" \
  2>"$outdir/bpftrace_stderr.txt"

printf '%s\n' "$outdir"
