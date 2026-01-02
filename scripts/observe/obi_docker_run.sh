#!/usr/bin/env bash
set -euo pipefail

# Run OBI (OpenTelemetry eBPF Instrumentation) + OTel Collector in Docker
# and observe minihttpd on host. Logs are stored under artifacts/obi/.

root_dir="$(pwd -P)"
ts="$(date +%Y%m%d-%H%M%S)"
outdir="$root_dir/artifacts/obi/$ts"
mkdir -p "$outdir"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker not found in PATH" >&2
  exit 1
fi

if [[ ! -x "$root_dir/minihttpd/minihttpd" ]]; then
  echo "minihttpd binary not found: $root_dir/minihttpd/minihttpd" >&2
  exit 1
fi

otelcol_name="otelcol-$ts"
obi_name="obi-$ts"
minihttpd_name="minihttpd-$ts"

# OBI tuning (override via environment)
obi_open_port="${OBI_OPEN_PORT:-8080}"
obi_auto_target_exe="${OBI_AUTO_TARGET_EXE:-*minihttpd}"
obi_log_level="${OBI_LOG_LEVEL:-DEBUG}"
obi_trace_printer="${OBI_TRACE_PRINTER:-text}"
obi_traces_instrumentations="${OBI_TRACES_INSTRUMENTATIONS:-*}"
obi_metrics_instrumentations="${OBI_METRICS_INSTRUMENTATIONS:-*}"

cleanup() {
  docker rm -f "$obi_name" >/dev/null 2>&1 || true
  docker rm -f "$otelcol_name" >/dev/null 2>&1 || true
  docker rm -f "$minihttpd_name" >/dev/null 2>&1 || true
}
trap cleanup EXIT

cat >"$outdir/collector-config.yaml" <<'EOF_CFG'
receivers:
  otlp:
    protocols:
      grpc:
        endpoint: 0.0.0.0:4317
      http:
        endpoint: 0.0.0.0:4318
exporters:
  debug:
    verbosity: detailed
service:
  pipelines:
    traces:
      receivers: [otlp]
      exporters: [debug]
    metrics:
      receivers: [otlp]
      exporters: [debug]
    logs:
      receivers: [otlp]
      exporters: [debug]
EOF_CFG

# Pre-pull images to avoid cutting logs mid-run
docker pull otel/opentelemetry-collector >/dev/null
docker pull docker.io/otel/ebpf-instrument:main >/dev/null

# Start OTel Collector in Docker (host network)
docker run -d --name "$otelcol_name" \
  --network=host \
  -v "$outdir/collector-config.yaml":/etc/otelcol/config.yaml:ro \
  otel/opentelemetry-collector >/dev/null

# Start OBI in Docker (host network + host PID + privileged)
docker run -d --name "$obi_name" \
  --network=host \
  --pid=host \
  --privileged \
  -v /sys/fs/cgroup:/sys/fs/cgroup:ro \
  -v /sys/kernel/security:/sys/kernel/security:ro \
  -e OTEL_EBPF_OPEN_PORT="$obi_open_port" \
  -e OTEL_EBPF_AUTO_TARGET_EXE="$obi_auto_target_exe" \
  -e OTEL_EBPF_LOG_LEVEL="$obi_log_level" \
  -e OTEL_EBPF_TRACE_PRINTER="$obi_trace_printer" \
  -e OTEL_EBPF_TRACES_INSTRUMENTATIONS="$obi_traces_instrumentations" \
  -e OTEL_EBPF_METRICS_INSTRUMENTATIONS="$obi_metrics_instrumentations" \
  -e OTEL_EXPORTER_OTLP_ENDPOINT=http://127.0.0.1:4318 \
  docker.io/otel/ebpf-instrument:main >/dev/null

# Build minihttpd image if needed and start container (after OBI so it can attach)
docker build -q -t study-minihttpd-obi:local "$root_dir/minihttpd" >/dev/null
docker run -d --name "$minihttpd_name" --network=host --pid=host study-minihttpd-obi:local >/dev/null

# Give processes time to start and be discovered
sleep 4

# Generate one request
/usr/bin/curl -sS http://127.0.0.1:8080/ >"$outdir/curl.out" 2>"$outdir/curl.err" || true

# Allow traces/metrics to flush
sleep 3

# Capture container logs
docker logs "$otelcol_name" >"$outdir/otelcol.log" 2>&1 || true
docker logs "$obi_name" >"$outdir/obi.log" 2>&1 || true
docker logs "$minihttpd_name" >"$outdir/minihttpd_stdout.txt" 2>"$outdir/minihttpd_stderr.txt" || true

printf '%s\n' "$outdir"
