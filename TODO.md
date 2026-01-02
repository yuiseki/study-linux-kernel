straceで見ている世界を eBPF/bpftrace に拡張（観測の高速道路）

ねらい：minishell/minihttpd の“見える化”をカーネル寄りへ

MVP：openat/read/write/connect/accept を bpftrace でトレースし、REPL/HTTPアクセスで差分を見る（syscalls tracepointが扱いやすい）

---

OpenTelemetryのeBPF自動計装で“ゼロ改修観測”を試す

ねらい：アプリ改修ゼロでRED指標/トレースが出る世界観の理解

MVP：OBI（OpenTelemetry eBPF Instrumentation）でHTTP/gRPC相当の観測を動かす

発展：minihttpd に当て、手元環境のボトルネック探索に使う

※メモ: 本リポジトリは C 実装（minishell/minihttpd）。OBI はプロセス検出はできたが
HTTP トレースが出ず、現状は “非対応/部分対応” と判断。OpenTelemetry 系は以後対象外とする。

---

“全体プロファイラ”としての eBPF を触る（CPU/スタック）

ねらい：syscall列だけでなく「CPU時間がどこに消えたか」を掴む

MVP：OpenTelemetryの eBPF profiler をQuickStartで動かす
