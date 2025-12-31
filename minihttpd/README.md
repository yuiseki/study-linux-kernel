# minihttpd

最小・観察可能を目的にした HTTP サーバ実装です。`hello, world!` を返すだけの
最小サーバで、学習用に socket の基本操作が追えることを重視しています。

## ビルド

```sh
make
```

生成物は `./minihttpd` です。

## 実行

```sh
./minihttpd
```

別ターミナルでアクセスします:

```sh
curl -v http://localhost:8080/
```

ブラウザでも `http://localhost:8080/` を開くと表示されます。
終了するときは `Ctrl+C` で止めてください。

## トレース実行

`strace` を内包して syscall ログを出したい場合は `--trace` を使います。

```sh
./minihttpd --trace
```

ログは `./logs/minihttpd/<timestamp>-<pid>/trace.txt` に保存されます
（作成できない場合は `./tmp/minihttpd/` に作成します）。

## 観測のヒント

`strace` で syscall を観測できます。

```sh
strace -ff -o /tmp/trace ./minihttpd
```

`socket/bind/listen/accept/read/write` の流れが確認できます。
