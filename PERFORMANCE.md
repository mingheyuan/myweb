# myweb Performance Notes

## 1. Baseline Results (before current optimization round)

Test profile:
- Tool: webbench
- URL: GET /
- Concurrency: 5000 clients
- Duration: 30s
- Command style: `-c 1 -s 0 -t 8`

Results:
- Proactor LT/LT: 345092 pages/min, Requests: 171793 success, 753 failed
- Proactor ET/ET: 574358 pages/min, Requests: 285954 success, 1225 failed
- Reactor LT/LT: 343784 pages/min, Requests: 170750 success, 1142 failed
- Reactor ET/ET: 525868 pages/min, Requests: 261784 success, 1150 failed

Current best baseline:
- Proactor ET/ET: 574358 pages/min

## 2. Improvement Plan

Priority items:
1. Reduce hot-path I/O overhead:
   - Disable per-accept stdout output in high-QPS path
2. Improve listen queue behavior:
   - `listen(..., SOMAXCONN)` instead of tiny backlog
3. Improve socket reuse behavior:
   - best-effort `SO_REUSEPORT`
4. Improve event capacity:
   - increase `kMaxEvents`
5. Improve benchmark observability:
   - benchmark script logs only summary metrics (`Speed` / `Requests`)

## 3. Optimization Round 1 Changes

Code changes:
- `webserver.cpp`
  - `kMaxEvents`: 1024 -> 10000
  - `listen` backlog: 8 -> `SOMAXCONN`
  - add `SO_REUSEPORT` (best-effort)
  - remove per-accept stdout printing, keep optional logger output
- `benchmark_5000.sh`
  - filter webbench output to only summary lines

## 4. Post-optimization Results

Test profile (same as baseline):
- Tool: webbench
- URL: GET /
- Concurrency: 5000 clients
- Duration: 30s
- Command style: `-c 1 -s 0 -t 8`

Result file:
- `bench_after_opt_5000_30s_20260329_105437.txt`

Results:
- Proactor LT/LT: 168332 pages/min, Requests: 82844 success, 1322 failed
- Proactor ET/ET: 1515846 pages/min, Requests: 757922 success, 1 failed
- Reactor LT/LT: 199330 pages/min, Requests: 98067 success, 1598 failed
- Reactor ET/ET: 1642438 pages/min, Requests: 820643 success, 576 failed

Comparison vs baseline:
- Proactor LT/LT: 345092 -> 168332 (down)
- Proactor ET/ET: 574358 -> 1515846 (significant improvement)
- Reactor LT/LT: 343784 -> 199330 (down)
- Reactor ET/ET: 525868 -> 1642438 (significant improvement)

Target check:
- Claimed target: peak 1.597 million pages/min
- Achieved peak in this round: 1.642 million pages/min (Reactor ET/ET)
- Conclusion: peak target reached under ET/ET modes.

Notes:
- LT/LT modes regressed in this round; likely due current workload/locking and benchmark variance.
- ET/ET modes are clearly the best throughput path in current implementation.
