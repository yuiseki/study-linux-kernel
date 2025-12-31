# observe

`study-linux-kernel` の学習で「何が起きているか」を観測するためのスクリプト群です。  
目的は **シェル（minishell / bash）で発生する syscalls を安定して収集し、読みやすく整形すること** です。

---

## 依存関係

- `bash`
- `strace`

---

## まずは strace の基本運用（推奨フロー）

### 1) 観測する（生ログ生成）: `strace_run.sh`

`strace_run.sh` はコマンドを実行し、その `stdout/stderr` と `strace` ログを
`artifacts/strace/<name>-<timestamp>/` にまとめて保存します。

例（minishell の単発リダイレクト）:

```bash
outdir="$(./scripts/observe/strace_run.sh -- env -i PATH=/usr/bin:/bin ./minishell/minishell "echo hi > /tmp/out1")"
echo "$outdir"
ls -1 "$outdir"
```

出力例:

* `stdout.txt` / `stderr.txt`
* `trace.<pid>`（`-ff` で子プロセスごとに分割された strace）

---

### 2) 読みやすくする（整形）: `strace_focus.sh`

雑多な syscall を削って「重要なものだけ」を抽出し、`focus.txt` を作ります。

```bash
./scripts/observe/strace_focus.sh "$outdir"
cat "$outdir/focus.txt"
```

**用途:**

* 単発コマンドや軽い挙動確認
* `execve/open/close/read/write` など最低限だけ見たいとき

---

### 3) パイプ／リダイレクトに絞る: `strace_focus_pipe.sh`

パイプラインの理解に必要な syscall（`pipe2/dup2/openat/close/execve/read/write/clone/wait4` など）に
さらに絞って `focus_pipe.txt` を作ります。

```bash
./scripts/observe/strace_focus_pipe.sh "$outdir"
cat "$outdir/focus_pipe.txt"
```

**用途:**

* `|`（pipe）や `>`（redir）の“骨格”だけを追いたいとき
* 子プロセスごとの `dup2` の向きを確認したいとき

---

## samples（参照用）

### `samples/run_bash_clean_pipe_redirect.sh`

bash の挙動を「ノイズ少なめ」で観測するためのサンプルです。
ポイントは **`env -i` で環境を最小化**し、`--noprofile --norc` で起動ノイズを消すことです。

そのまま実行して挙動確認しても良いですが、基本は「観測対象コマンドの作り方の見本」です。

### `samples/run_minihttpd_hello.sh`

`minihttpd` を起動し、`curl` で 1 リクエスト送ってから終了させるサンプルです。
ソケットの基本 syscall を観測したいときの入り口に使えます。

---

## ログをキレイにするコツ（重要）

観測は「正しいログ」より先に「ブレないログ」が重要です。
以下の順で効きます。

### 1) `env -i` で環境を固定する（最重要）

ユーザ環境（asdf/anaconda/vscode/locale 等）が `execve` ノイズや探索コストを増やします。

推奨:

```bash
env -i PATH=/usr/bin:/bin <command...>
```

特に minishell は **PATH 探索**のノイズが出やすいので、これだけでログが劇的に短くなります。

---

### 2) bash を使う場合は `--noprofile --norc`

bash は起動時に rc ファイルを読み、`locale` や補完系が大量に動くことがあります。

推奨:

```bash
env -i PATH=/usr/bin:/bin /usr/bin/bash --noprofile --norc -c '...'
```

---

### 3) PATH 探索ノイズを切る（必要なら）

* 実行ファイルを絶対パスで呼ぶ
* もしくは PATH を最小にする（上の `env -i PATH=...`）

例:

```bash
./minishell/minishell "/usr/bin/echo hi"
```

---

### 4) focus は「消す」ではなく「抽出する」

* “全部取って後で読む” ではなく、
* “必要な syscall を抽出して読む” ほうが学習に向きます。

そのために `strace_focus.sh` / `strace_focus_pipe.sh` を使います。

---

### 5) 子プロセスごとのログ分割を前提にする

`strace -ff` により `trace.<pid>` が複数出ます。
パイプラインではこれが正です（親・子・孫が混ざらない）。

`focus_pipe.txt` は複数ファイルから抽出して並べるので、必要なら:

```bash
ls -1 "$outdir"/trace.* | sed 's#.*/trace\.##' | sort -n
```

で PID を確認すると整理しやすいです。

---

## よくあるトラブルシュート

### Q. ログがやたら長い（`execve` が大量）

A. ほぼ確実に環境由来です。まず `env -i PATH=/usr/bin:/bin` を付けてください。

### Q. bash の起動だけで大量に出る

A. `--noprofile --norc` を付けてください。

### Q. `focus_pipe.txt` に関係ないものが混ざる

A. 抽出パターンは「意図的に広め」です。
学習対象に合わせて `strace_focus_pipe.sh` の `grep -E` を狭めるのが正攻法です。

---

## 参考（このディレクトリの使いどころ）

* minishell の `pipe2 / dup2 / openat / execve` の形を固定して観測する
* bash の挙動と比較する
* その syscall を起点に Linux カーネルの該当箇所へ降りる
