#!/usr/bin/env bash
set -euo pipefail

./scripts/observe/bpftrace_run.sh -- \
  ./scripts/observe/samples/run_minihttpd_hello.sh
