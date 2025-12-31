# minishell

学習目的で実装した小さなシェルです。UNIX シェルの基本的な仕組み（プロセス生成、パイプ、リダイレクト）を理解しながら実装・検証できることを目指しています。

## 特徴

- シンプルな構成で、シェルの基礎動作を追いやすい
- パイプや出力リダイレクト（行末の `>` のみ）など、最小限のシェル機能に絞っている
- `strace` を組み込み、REPL から `:trace` でシステムコール観測ログを出力できる

## ビルド方法

```sh
make
```

生成物は `./minishell` です。

## 基本的な使い方

```sh
# REPL
./minishell

# 一発実行
./minishell "echo hi | wc -c > /tmp/out"
```

REPL では以下の最小 built-in が使えます。

- `exit` / `quit`: 終了
- `:trace on|off`: strace の有効/無効
- `:trace pipe|all`: 追跡モード切り替え（pipe はパイプ/リダイレクト中心、all は広め）
- `:trace`: 現在の状態表示
