#!/usr/bin/env python3
"""Measure sampling-profiler overhead with a fixed-work benchmark.

Configurations (same QEMU flags, RAM, CPUS, image layout):
  stock      xv6 at the baseline commit (7627784) + only user/kbench (-DSTOCK_XV6)
  prof-off   toolkit kernel, profiler stopped
  prof-on    toolkit kernel, `kprof start` before the workload

Each (config, workload, cpus) point is run --runs times; raw per-run ticks go to
<out>/raw.csv, mean/stdev/min/max to <out>/summary.csv. Timer resolution is
1 tick = ~0.1 s, so pick workloads of several seconds. Overhead (%) is
(mean_cfg - mean_stock) / mean_stock * 100 and is only meaningful relative to the
reported spread. Run on an otherwise idle host.

  scripts/bench_kprof.py --runs 5
"""
import argparse
import csv
import os
import re
import shutil
import signal
import statistics
import subprocess
import sys
import time

STOCK_COMMIT = "7627784"

ap = argparse.ArgumentParser()
ap.add_argument(
    "--stock",
    default="/tmp/xv6-stock-bench",
    help="dir with a built stock kernel + fs.img (auto-created from baseline commit if missing)",
)
ap.add_argument("--toolkit", default=".", help="dir with the toolkit kernel + fs.img")
ap.add_argument("--runs", type=int, default=5)
ap.add_argument("--cpus", default="1,4")
ap.add_argument("--workloads", default="cpu 1500,sys 200,io 30")
ap.add_argument("--out", default="results/kprof-bench")
ap.add_argument("--timeout", type=int, default=300)
args = ap.parse_args()
os.makedirs(args.out, exist_ok=True)


def ensure_stock_build(stock_dir, toolkit_dir):
    """Auto-bootstrap stock xv6 baseline with user/kbench (-DSTOCK_XV6) if not built."""
    kpath = os.path.join(stock_dir, "kernel", "kernel")
    fspath = os.path.join(stock_dir, "fs.img")
    if os.path.exists(kpath) and os.path.exists(fspath):
        return

    print(f"[bench] Bootstrapping stock xv6 ({STOCK_COMMIT}) in {stock_dir} ...")
    if os.path.exists(stock_dir):
        shutil.rmtree(stock_dir)
    os.makedirs(stock_dir, exist_ok=True)

    p1 = subprocess.Popen(
        ["git", "archive", STOCK_COMMIT],
        cwd=toolkit_dir,
        stdout=subprocess.PIPE,
    )
    p2 = subprocess.Popen(
        ["tar", "-x", "-C", stock_dir],
        stdin=p1.stdout,
    )
    p1.stdout.close()
    rc2 = p2.wait()
    rc1 = p1.wait()
    if rc1 != 0 or rc2 != 0:
        raise RuntimeError(f"git archive {STOCK_COMMIT} failed ({rc1}, {rc2})")

    shutil.copy2(
        os.path.join(toolkit_dir, "user", "kbench.c"),
        os.path.join(stock_dir, "user", "kbench.c"),
    )

    mk_path = os.path.join(stock_dir, "Makefile")
    with open(mk_path, "r") as f:
        mk = f.read()

    if "-DSTOCK_XV6" not in mk:
        mk = mk.replace("CFLAGS += $(DETFLAGS)\n", "CFLAGS += $(DETFLAGS) -DSTOCK_XV6\n", 1)
    if "$U/_kbench" not in mk:
        mk = mk.replace("\t$U/_sync\\\n", "\t$U/_sync\\\n\t$U/_kbench\\\n", 1)

    with open(mk_path, "w") as f:
        f.write(mk)

    build = subprocess.run(
        ["make", "-j4", "kernel/kernel", "fs.img"],
        cwd=stock_dir,
        capture_output=True,
        text=True,
    )
    if build.returncode != 0:
        print(build.stdout[-1000:])
        print(build.stderr[-1000:])
        raise RuntimeError("Failed to build stock xv6 baseline")
    print(f"[bench] Built stock xv6 baseline cleanly in {stock_dir}")


def run_vm(d, cpus, pre, cmd):
    """Boot xv6 in d, run optional pre-command then cmd, return ticks or None."""
    p = subprocess.Popen(
        ["make", "qemu", "CPUS=%d" % cpus],
        cwd=d,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        preexec_fn=os.setsid,
    )
    os.set_blocking(p.stdout.fileno(), False)
    out, stage, t0 = "", 0, time.time()
    ticks = None
    while time.time() - t0 < args.timeout:
        try:
            c = p.stdout.read(4096)
        except BlockingIOError:
            c = None
        if c:
            out += c.decode(errors="replace")
        else:
            time.sleep(0.05)
        if out.rstrip().endswith("$") and stage < 2:
            seq = [pre, cmd] if pre else [cmd]
            if stage < len(seq):
                p.stdin.write((seq[stage] + "\n").encode())
                p.stdin.flush()
                out += "\n"  # avoid re-triggering on the same prompt
                stage += 1
        m = re.search(r"kbench: .* ticks=(\d+)", out)
        if m:
            ticks = int(m.group(1))
            break
        if "panic" in out:
            break
    try:
        os.killpg(os.getpgid(p.pid), signal.SIGKILL)
    except ProcessLookupError:
        pass
    p.wait()
    return ticks, out


ensure_stock_build(args.stock, args.toolkit)

configs = [
    ("stock", args.stock, None),
    ("prof-off", args.toolkit, None),
    ("prof-on", args.toolkit, "kprof start"),
]
points = []
with open(os.path.join(args.out, "raw.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["config", "workload", "cpus", "run", "ticks"])
    # interleave configs inside each run to spread host-load drift evenly
    for cpus in [int(c) for c in args.cpus.split(",")]:
        for wl in [w_s.strip() for w_s in args.workloads.split(",") if w_s.strip()]:
            res = {name: [] for name, _, _ in configs}
            for r in range(1, args.runs + 1):
                for name, d, pre in configs:
                    ticks, out = run_vm(d, cpus, pre, "kbench " + wl)
                    if ticks is None:
                        print("FAILED run", name, wl, cpus, r, "\n", out[-800:])
                        sys.exit(1)
                    w.writerow([name, wl, cpus, r, ticks])
                    f.flush()
                    res[name].append(ticks)
                    print("%-9s cpus=%d %-10s run %d/%d: %d ticks" % (name, cpus, wl, r, args.runs, ticks))
            points.append((cpus, wl, res))

summary_rows = []
with open(os.path.join(args.out, "summary.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["cpus", "workload", "config", "n", "mean", "stdev", "min", "max", "overhead_pct_vs_stock"])
    for cpus, wl, res in points:
        base = statistics.mean(res["stock"])
        for name in res:
            v = res[name]
            sd = statistics.stdev(v) if len(v) > 1 else 0.0
            mean_v = statistics.mean(v)
            ov = ((mean_v - base) / base * 100.0) if base > 0 else 0.0
            row = [
                cpus,
                wl,
                name,
                len(v),
                "%.2f" % mean_v,
                "%.2f" % sd,
                min(v),
                max(v),
                "%.1f" % ov,
            ]
            w.writerow(row)
            summary_rows.append(row)

print("\n=== BENCHMARK SUMMARY (results/kprof-bench/summary.csv) ===")
print("%-4s  %-10s  %-9s  %4s  %12s  %6s  %9s  %17s" % (
    "CPUS", "Workload", "Config", "Runs", "Mean (ticks)", "Stdev", "Min..Max", "Overhead vs Stock"
))
print("-" * 82)
for row in summary_rows:
    cpus, wl, cfg, n, mean_s, sd_s, mn, mx, ov_s = row
    print("%-4s  %-10s  %-9s  %4s  %12s  %6s  %4s..%-4s  %16s%%" % (
        cpus, wl, cfg, n, mean_s, sd_s, mn, mx, ov_s
    ))
