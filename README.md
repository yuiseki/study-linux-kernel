# study-linux-kernel

Linux カーネルに関する学習用リポジトリ。

## Directories

- `minishell/`: パイプとリダイレクトが機能する最小シェル
- `minihttpd/`: ソケット学習用の最小 HTTP サーバ
- `scripts/observe/`: strace ログの取得と比較用スクリプト
- `artifacts/`: 生成物 (strace ログなど)
- `forks/`: 外部コードや実験用

## minishell

C 言語で書かれた、パイプとリダイレクトが機能するシェルの最小実装です。

strace が統合されており、コマンドを実行するたびにシステムコールをトレースすることができます。

### Build

```sh
cd minishell
make
```

### Run

```sh
./minishell/minishell
```

一発実行モード:

```sh
./minishell/minishell "echo hi | wc -c > /tmp/out"
```

### Observe (strace)

再現性のあるログを取りたい場合は `scripts/observe` を使います。

```sh
./scripts/observe/strace_run.sh -- env -i PATH=/usr/bin:/bin ./minishell/minishell "echo hi | wc -c"
```

## minihttpd

ソケットと HTTP の基本を学ぶための最小 HTTP サーバです。デフォルトで 8080 番ポートで待ち受けます。

### Build

```sh
cd minihttpd
make
```

### Run

```sh
./minihttpd/minihttpd
```

### Observe (strace)

```sh
./minihttpd/minihttpd --trace
```

## scripts/observe

`strace` のログを取得・フィルタリングして、挙動の差分を比較するための補助スクリプト群です。詳しくは
`scripts/observe/README.md` を参照してください。
