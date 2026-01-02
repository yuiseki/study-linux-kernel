#!/usr/bin/env bash
set -euo pipefail

./scripts/observe/bpftrace_run.sh -- \
  env -i PATH=/usr/bin:/bin \
  ./minishell/minishell "echo hi | wc -c > /tmp/minishell_out"
