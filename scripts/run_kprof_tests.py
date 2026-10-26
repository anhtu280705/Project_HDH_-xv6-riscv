#!/usr/bin/env python3
"""Run an xv6 user program under QEMU and collect its output.

  scripts/run_kprof_tests.py                      # kproftest, CPUS=3, 1 run
  scripts/run_kprof_tests.py --cpus 1 --repeat 10 # stress, one log per run
  scripts/run_kprof_tests.py --cmd "kprof status"

Exit status is 0 only if every run printed the done marker with no FAIL and
no panic. Raw logs go to results/kprof/ (one file per run, never overwritten
by a later passing run). Assumes kernel/kernel and fs.img are already built.
"""
import argparse, os, re, signal, subprocess, sys, time

ap = argparse.ArgumentParser()
ap.add_argument("--cpus", type=int, default=3)
ap.add_argument("--repeat", type=int, default=1)
ap.add_argument("--cmd", default="kproftest")
ap.add_argument("--done", default="kproftest: DONE", help="end-of-output marker")
ap.add_argument("--timeout", type=int, default=240)
ap.add_argument("--logdir", default="results/kprof")
args = ap.parse_args()

os.makedirs(args.logdir, exist_ok=True)
stamp = time.strftime("%Y%m%d-%H%M%S")


def run_once(i):
    p = subprocess.Popen(["make", "qemu", "CPUS=%d" % args.cpus],
                         stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, preexec_fn=os.setsid)
    os.set_blocking(p.stdout.fileno(), False)
    out = b""
    sent = False
    t0 = time.time()
    status = "TIMEOUT"
    while time.time() - t0 < args.timeout:
        try:
            chunk = p.stdout.read(4096)
        except BlockingIOError:
            chunk = None
        if chunk:
            out += chunk
        else:
            time.sleep(0.05)
        text = out.decode(errors="replace")
        if not sent and text.rstrip().endswith("$"):
            p.stdin.write((args.cmd + "\n").encode())
            p.stdin.flush()
            sent = True
        if "panic" in text:
            status = "PANIC"
            break
        if sent and args.done in text:
            status = "DONE"
            break
        if p.poll() is not None:
            status = "QEMU-EXIT"
            break
    elapsed = time.time() - t0
    try:
        os.killpg(os.getpgid(p.pid), signal.SIGKILL)
    except ProcessLookupError:
        pass
    p.wait()
    text = out.decode(errors="replace")
    path = os.path.join(args.logdir, "%s-cpus%d-run%02d.log" % (stamp, args.cpus, i))
    with open(path, "w") as f:
        f.write("# cmd=%s cpus=%d status=%s elapsed=%.1fs\n" % (args.cmd, args.cpus, status, elapsed))
        f.write(text)
    fails = len(re.findall(r"^\w+ FAIL ", text, re.M))
    good = status == "DONE" and fails == 0
    print("run %d/%d cpus=%d: %s fails=%d %.0fs log=%s" %
          (i, args.repeat, args.cpus, status, fails, elapsed, path))
    return good, text


all_ok = True
for i in range(1, args.repeat + 1):
    ok, text = run_once(i)
    all_ok &= ok
    if not ok:
        print(text[-1500:])
sys.exit(0 if all_ok else 1)

