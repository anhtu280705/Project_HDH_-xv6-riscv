#!/usr/bin/env python3
"""Measure sampling-profiler overhead with a fixed-work benchmark.

Configurations (same QEMU flags, RAM, CPUS, image layout):
  stock      xv6 at the baseline commit + only user/kbench (no toolkit code)
  prof-off   toolkit kernel, profiler stopped
  prof-on    toolkit kernel, `kprof start` before the workload

Each (config, workload, cpus) point is run --runs times; raw per-run ticks go to
<out>/raw.csv, mean/stdev/min/max to <out>/summary.csv. Timer resolution is
1 tick = ~0.1 s, so pick workloads of several seconds. Overhead (%) is
(mean_cfg - mean_stock) / mean_stock * 100 and is only meaningful relative to the
reported spread. Run on an otherwise idle host.

  scripts/bench_kprof.py --stock /tmp/xv6-stock --runs 5
"""
import argparse, csv, os, re, signal, statistics, subprocess, sys, time

ap = argparse.ArgumentParser()
ap.add_argument("--stock", required=True, help="dir with a built stock kernel + fs.img (with kbench)")
ap.add_argument("--toolkit", default=".", help="dir with the toolkit kernel + fs.img")
ap.add_argument("--runs", type=int, default=5)
ap.add_argument("--cpus", default="1,4")
ap.add_argument("--workloads", default="cpu 2000,sys 300")
ap.add_argument("--out", default="results/kprof-bench")
ap.add_argument("--timeout", type=int, default=300)
args = ap.parse_args()
os.makedirs(args.out, exist_ok=True)


def run_vm(d, cpus, pre, cmd):
    """Boot xv6 in d, run optional pre-command then cmd, return ticks or None."""
    p = subprocess.Popen(["make", "qemu", "CPUS=%d" % cpus], cwd=d,
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, preexec_fn=os.setsid)
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
                p.stdin.write((seq[stage] + "\n").encode()); p.stdin.flush()
                out += "\n"  # avoid re-triggering on the same prompt
                stage += 1
        m = re.search(r"kbench: .* ticks=(\d+)", out)
        if m:
            ticks = int(m.group(1)); break
        if "panic" in out:
            break
    try:
        os.killpg(os.getpgid(p.pid), signal.SIGKILL)
    except ProcessLookupError:
        pass
    p.wait()
    return ticks, out


configs = [("stock", args.stock, None),
           ("prof-off", args.toolkit, None),
           ("prof-on", args.toolkit, "kprof start")]
points = []
with open(os.path.join(args.out, "raw.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["config", "workload", "cpus", "run", "ticks"])
    # interleave configs inside each run to spread host-load drift evenly
    for cpus in [int(c) for c in args.cpus.split(",")]:
        for wl in args.workloads.split(","):
            res = {name: [] for name, _, _ in configs}
            for r in range(1, args.runs + 1):
                for name, d, pre in configs:
                    ticks, out = run_vm(d, cpus, pre, "kbench " + wl)
                    if ticks is None:
                        print("FAILED run", name, wl, cpus, r, "\n", out[-800:])
                        sys.exit(1)
                    w.writerow([name, wl, cpus, r, ticks]); f.flush()
                    res[name].append(ticks)
                    print("%-9s cpus=%d %-8s run %d: %d ticks" % (name, cpus, wl, r, ticks))
            points.append((cpus, wl, res))

with open(os.path.join(args.out, "summary.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["cpus", "workload", "config", "n", "mean", "stdev", "min", "max", "overhead_pct_vs_stock"])
    for cpus, wl, res in points:
        base = statistics.mean(res["stock"])
        for name in res:
            v = res[name]
            sd = statistics.stdev(v) if len(v) > 1 else 0.0
            w.writerow([cpus, wl, name, len(v), "%.2f" % statistics.mean(v), "%.2f" % sd,
                        min(v), max(v), "%.1f" % ((statistics.mean(v) - base) / base * 100)])
print(open(os.path.join(args.out, "summary.csv")).read())

