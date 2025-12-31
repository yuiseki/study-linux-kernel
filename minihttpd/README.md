# minihttpd

最小・観察可能を目的にした HTTP サーバ実装です。1 リクエストだけ受け付けて
`hello, world!` を返したら終了します。学習用に socket の基本操作が追えることを
重視しています。

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

## 観測のヒント

`strace` で syscall を観測できます。

```sh
strace -ff -o /tmp/trace ./minihttpd
```

`socket/bind/listen/accept/read/write` の流れが確認できます。
