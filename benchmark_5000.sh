#!/usr/bin/env bash
set -euo pipefail

# Default settings
CLIENTS="${CLIENTS:-5000}"
DURATION="${DURATION:-30}"
THREADS="${THREADS:-8}"
START_PORT="${START_PORT:-18120}"
LOG_DISABLED="${LOG_DISABLED:-1}"
SQL_POOL="${SQL_POOL:-0}"

WB_BIN="${WB_BIN:-/home/yuan/studyweb/TinyWebServer/test_pressure/webbench-1.5/webbench}"
SERVER_BIN="${SERVER_BIN:-./server}"

if [[ ! -x "$WB_BIN" ]]; then
  echo "ERROR: webbench not found or not executable: $WB_BIN"
  exit 1
fi

if [[ ! -x "$SERVER_BIN" ]]; then
  echo "ERROR: server binary not found: $SERVER_BIN"
  echo "Hint: run ./build.sh first"
  exit 1
fi

OUT_FILE="${OUT_FILE:-bench_results_${CLIENTS}_${DURATION}s_$(date +%Y%m%d_%H%M%S).txt}"

PIDS=()
cleanup() {
  for pid in "${PIDS[@]:-}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  done
}
trap cleanup EXIT INT TERM

run_case() {
  local label="$1"
  local port="$2"
  local trig_mode="$3"
  local actor_model="$4"

  echo "===== ${label} =====" | tee -a "$OUT_FILE"
  echo "port=${port}, clients=${CLIENTS}, duration=${DURATION}s, m=${trig_mode}, a=${actor_model}" | tee -a "$OUT_FILE"

  "$SERVER_BIN" \
    -p "$port" \
    -c "$LOG_DISABLED" \
    -s "$SQL_POOL" \
    -m "$trig_mode" \
    -a "$actor_model" \
    -t "$THREADS" \
    >/tmp/myweb_server_${port}.log 2>&1 &

  local spid=$!
  PIDS+=("$spid")

  sleep 1

  if ! curl -s "http://127.0.0.1:${port}/" >/dev/null; then
    echo "ERROR: server did not respond on port ${port}" | tee -a "$OUT_FILE"
    return 1
  fi

  set +e
  "$WB_BIN" -1 --get -c "$CLIENTS" -t "$DURATION" "http://127.0.0.1:${port}/" \
    | grep -E 'Speed=|Requests:' \
    | tee -a "$OUT_FILE"
  set -e
  echo | tee -a "$OUT_FILE"

  if kill -0 "$spid" 2>/dev/null; then
    kill "$spid" 2>/dev/null || true
    wait "$spid" 2>/dev/null || true
  fi
}

: > "$OUT_FILE"

echo "Benchmark started at $(date '+%F %T')" | tee -a "$OUT_FILE"
echo "webbench=${WB_BIN}" | tee -a "$OUT_FILE"
echo "server=${SERVER_BIN}" | tee -a "$OUT_FILE"
echo | tee -a "$OUT_FILE"

# Matrix:
# m=0: LT/LT, m=3: ET/ET
# a=0: Proactor, a=1: Reactor
run_case "Proactor LT/LT" "$START_PORT" 0 0
run_case "Proactor ET/ET" "$((START_PORT + 1))" 3 0
run_case "Reactor LT/LT" "$((START_PORT + 2))" 0 1
run_case "Reactor ET/ET" "$((START_PORT + 3))" 3 1

echo "Done. Results saved to: $OUT_FILE"
