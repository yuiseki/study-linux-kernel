#!/usr/bin/bash
set -euo pipefail

# env(1)を挟むと PATH 探索で execve が増えるので、bash を絶対パスで直起動する
env -i PATH=/usr/bin:/bin \
  /usr/bin/bash --noprofile --norc -c 'echo hi | wc -c > /tmp/minishell_out'
