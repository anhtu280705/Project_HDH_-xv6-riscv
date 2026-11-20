#!/usr/bin/env python3
"""One-command weekly evidence generator for Nguyen Anh Tu's kprof module (Weeks 5-8).

Usage:
  python3 scripts/tu_weekly_evidence.py week5   # Checkpoint: hot-loop demo + CPUS=3,4 kproftest
  python3 scripts/tu_weekly_evidence.py week6   # Stress: 10x CPUS=1 + 10x CPUS=4 (300 test cases)
  python3 scripts/tu_weekly_evidence.py week7   # Benchmark: stock vs prof-off vs prof-on (5x each)
  python3 scripts/tu_weekly_evidence.py week8   # Code freeze: clean build, symbol verify, SHA256, crash/kproftest
"""
import argparse
import csv
import hashlib
import os
import re
import signal
import subprocess
import sys
import time

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def run_host(cmd, check=True):
    print(f"+ {' '.join(cmd)}")
    r = subprocess.run(cmd, cwd=REPO_ROOT)
    if check and r.returncode != 0:
        sys.exit(r.returncode)
    return r.returncode


def run_host_capture(cmd):
    print(f"+ {' '.join(cmd)}")
    p = subprocess.Popen(
        cmd,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )
    lines = []
    for line in p.stdout:
        sys.stdout.write(line)
        sys.stdout.flush()
        lines.append(line)
    rc = p.wait()
    return rc, "".join(lines)


def ensure_built():
    kpath = os.path.join(REPO_ROOT, "kernel", "kernel")
    fspath = os.path.join(REPO_ROOT, "fs.img")
    if not (os.path.exists(kpath) and os.path.exists(fspath)):
        run_host(["make", "-j4", "kernel/kernel", "fs.img"])


def run_qemu_seq(cpus, cmds, timeout=180):
    """Boot xv6 with CPUS=<cpus>, run commands one by one waiting for shell '$ ' prompt."""
    p = subprocess.Popen(
        ["make", "qemu", f"CPUS={cpus}"],
        cwd=REPO_ROOT,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        preexec_fn=os.setsid,
    )
    os.set_blocking(p.stdout.fileno(), False)

    full_out = ""
    stage = -1  # -1: waiting for initial boot prompt
    last_len = 0
    t0 = time.time()

    try:
        while time.time() - t0 < timeout:
            try:
                chunk = p.stdout.read(4096)
            except BlockingIOError:
                chunk = None

            if chunk:
                s = chunk.decode(errors="replace")
                full_out += s
            else:
                time.sleep(0.05)

            if "panic:" in full_out:
                break

            ready_for_prompt = full_out.rstrip().endswith("$") and len(full_out) > last_len
            if stage == -1:
                if "init: starting sh" in full_out and full_out.rstrip().endswith("$"):
                    time.sleep(0.25)
                    # Drain any late hart startup messages
                    try:
                        extra = p.stdout.read(4096)
                        if extra:
                            full_out += extra.decode(errors="replace")
                    except BlockingIOError:
                        pass
                    stage = 0
                    last_len = len(full_out)
                    p.stdin.write((cmds[stage] + "\n").encode())
                    p.stdin.flush()
            elif ready_for_prompt:
                stage += 1
                if stage >= len(cmds):
                    break
                last_len = len(full_out)
                p.stdin.write((cmds[stage] + "\n").encode())
                p.stdin.flush()

            if p.poll() is not None:
                break
    finally:
        try:
            os.killpg(os.getpgid(p.pid), signal.SIGKILL)
        except ProcessLookupError:
            pass
        p.wait()

    return full_out


def cmd_week5():
    print("========================================================================")
    print(" TUẦN 5 — CHECKPOINT DEMO: HOT-LOOP DETECTION & MULTI-CPU VERIFICATION")
    print("========================================================================")
    ensure_built()

    os.makedirs(os.path.join(REPO_ROOT, "results", "kprof"), exist_ok=True)
    log_path = os.path.join(REPO_ROOT, "results", "kprof", "week5-checkpoint.log")

    print("\n[1/3] Demo 1 (Kernel Hot-Loop Detection trên QEMU CPUS=4) ...")
    demo1_out = run_qemu_seq(
        4,
        [
            "kprof reset",
            "kprof start",
            "kbench kspin 35",
            "kprof stop",
            "kprof status",
            "kprof fn 5",
            "kprof top 5",
        ],
        timeout=90,
    )
    print(demo1_out.strip())

    print("\n[2/3] Demo 2 (User Hot-Loop & Syscall Workload trên QEMU CPUS=4) ...")
    demo2_out = run_qemu_seq(
        4,
        [
            "kprof reset",
            "kprof start",
            "kbench cpu 500",
            "kbench sys 80",
            "kprof stop",
            "kprof status",
            "kprof fn 8",
            "kprof top 8",
        ],
        timeout=90,
    )
    print(demo2_out.strip())

    print("\n[3/3] Kiểm chứng kproftest (15/15 PASS) trên CPUS=3 và CPUS=4 ...")
    rc3, out3 = run_host_capture(["python3", "scripts/run_kprof_tests.py", "--cpus", "3", "--repeat", "1"])
    rc4, out4 = run_host_capture(["python3", "scripts/run_kprof_tests.py", "--cpus", "4", "--repeat", "1"])

    with open(log_path, "w") as f:
        f.write("=== DEMO 1: KERNEL HOT-LOOP (CPUS=4) ===\n")
        f.write(demo1_out + "\n\n")
        f.write("=== DEMO 2: USER CPU + SYSCALL WORKLOAD (CPUS=4) ===\n")
        f.write(demo2_out + "\n\n")
        f.write("=== KPROFTEST CPUS=3 ===\n" + out3 + "\n")
        f.write("=== KPROFTEST CPUS=4 ===\n" + out4 + "\n")

    kspin_ok = "kprof_spin" in demo1_out
    user_ok = "<user>" in demo2_out
    all_ok = (rc3 == 0) and (rc4 == 0) and kspin_ok and user_ok

    print("\n=== TỔNG KẾT CHECKPOINT TUẦN 5 (TÚ - KPROF) ===")
    print(f"  - Kernel Hot-Loop Detection (kprof_spin resolved) : {'PASS' if kspin_ok else 'FAIL'}")
    print(f"  - User Hot-Loop & Syscall Isolation (<user>)       : {'PASS' if user_ok else 'FAIL'}")
    print(f"  - Multi-CPU Regression (CPUS=3)                    : {'15/15 PASS' if rc3 == 0 else 'FAIL'}")
    print(f"  - Multi-CPU Regression (CPUS=4)                    : {'15/15 PASS' if rc4 == 0 else 'FAIL'}")
    print(f"  - Raw Log saved to                                 : {log_path}")
    if not all_ok:
        sys.exit(1)


def cmd_week6():
    print("========================================================================")
    print(" TUẦN 6 — STRESS TESTING: 10x CPUS=1 + 10x CPUS=4 (300 TEST CASES)")
    print("========================================================================")
    ensure_built()
    stress_dir = os.path.join(REPO_ROOT, "results", "kprof-stress")
    os.makedirs(stress_dir, exist_ok=True)

    print("\n[1/2] Running 10x kproftest on CPUS=1 ...")
    rc1, out1 = run_host_capture(
        ["python3", "scripts/run_kprof_tests.py", "--cpus", "1", "--repeat", "10", "--logdir", "results/kprof-stress"]
    )
    print("\n[2/2] Running 10x kproftest on CPUS=4 ...")
    rc4, out4 = run_host_capture(
        ["python3", "scripts/run_kprof_tests.py", "--cpus", "4", "--repeat", "10", "--logdir", "results/kprof-stress"]
    )

    log_paths = re.findall(r"log=(\S+)", out1 + "\n" + out4)
    rows = []
    total_pass_cases = 0
    for lp in log_paths:
        full_lp = lp if os.path.isabs(lp) else os.path.join(REPO_ROOT, lp)
        txt = open(full_lp).read() if os.path.exists(full_lp) else ""
        m = re.search(r"kproftest:\s*(\d+)/(\d+)\s+PASS", txt)
        passed = int(m.group(1)) if m else 0
        total = int(m.group(2)) if m else 15
        total_pass_cases += passed
        m_hdr = re.search(r"cpus=(\d+)\s+status=(\S+)\s+elapsed=(\S+)", txt)
        cpus = m_hdr.group(1) if m_hdr else "?"
        status = m_hdr.group(2) if m_hdr else "?"
        elapsed = m_hdr.group(3) if m_hdr else "?"
        rows.append((os.path.basename(lp), cpus, status, f"{passed}/{total}", elapsed))

    summary_path = os.path.join(stress_dir, "summary.txt")
    lines = [
        "=== WEEK 6 KPROF STRESS SUMMARY (10x CPUS=1 + 10x CPUS=4) ===",
        "%-34s  %4s  %8s  %8s  %8s" % ("Log File", "CPUS", "Status", "Passed", "Elapsed"),
        "-" * 72,
    ]
    for fn, cpus, st, pstr, el in rows:
        lines.append("%-34s  %4s  %8s  %8s  %8s" % (fn, cpus, st, pstr, el))
    lines.append("-" * 72)
    lines.append(f"Total Runs: {len(rows)}/20 | Total Test Cases Passed: {total_pass_cases}/300")
    summary_text = "\n".join(lines)
    with open(summary_path, "w") as f:
        f.write(summary_text + "\n")

    print("\n" + summary_text)
    print(f"Saved summary to: {summary_path}")
    if rc1 != 0 or rc4 != 0 or total_pass_cases < 300:
        sys.exit(1)


def cmd_week7():
    print("========================================================================")
    print(" TUẦN 7 — SCIENTIFIC OVERHEAD BENCHMARK (STOCK vs PROF-OFF vs PROF-ON)")
    print("========================================================================")
    ensure_built()
    bench_out = os.path.join(REPO_ROOT, "results", "kprof-bench")

    rc = run_host(
        [
            "python3",
            "scripts/bench_kprof.py",
            "--runs",
            "5",
            "--cpus",
            "1,4",
            "--workloads",
            "cpu 500,sys 50,io 20",
            "--out",
            "results/kprof-bench",
        ],
        check=False,
    )
    if rc != 0:
        sys.exit(rc)

    sum_csv = os.path.join(bench_out, "summary.csv")
    print("\n=== GHI CHÚ PHÂN TÍCH (BÁO CÁO TUẦN 7) ===")
    print("  1. Đơn vị đo: 1 tick ~ 0.1s (timer xv6 uptime()).")
    print("  2. So sánh cùng điều kiện: QEMU -m 128M, cùng compiler flags (-O -fno-omit-frame-pointer).")
    print("  3. Khi chênh lệch < 1 tick, nhiễu làm tròn lượng tử hóa (quantization +/- 1 tick) và")
    print("     host scheduling noise chiếm ưu thế; cần diễn giải kèm stdev và min..max.")
    print(f"  4. Dữ liệu chi tiết lưu tại: {bench_out}/raw.csv và {sum_csv}")


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(65536)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def cmd_week8():
    print("========================================================================")
    print(" TUẦN 8 — CODE FREEZE: CLEAN REBUILD, SYMBOL DRIFT & REGRESSION")
    print("========================================================================")
    os.makedirs(os.path.join(REPO_ROOT, "results", "kprof"), exist_ok=True)
    log_path = os.path.join(REPO_ROOT, "results", "kprof", "week8-freeze.log")

    print("\n[1/4] Clean rebuild ...")
    run_host(["make", "clean"])
    rc_build, out_build = run_host_capture(["make", "-j4", "kernel/kernel", "fs.img"])
    if rc_build != 0:
        sys.exit(rc_build)

    print("\n[2/4] Zero Symbol Drift check ...")
    rc_sym, out_sym = run_host_capture(
        ["python3", "tools/gen_ksym.py", "kernel/kernel.base", "--verify", "kernel/kernel"]
    )
    if rc_sym != 0:
        sys.exit(rc_sym)

    print("\n[3/4] Running kproftest on CPUS=1 and CPUS=4 ...")
    rc1, out1 = run_host_capture(["python3", "scripts/run_kprof_tests.py", "--cpus", "1", "--repeat", "1"])
    rc4, out4 = run_host_capture(["python3", "scripts/run_kprof_tests.py", "--cpus", "4", "--repeat", "1"])

    print("\n[4/4] Running ./test-xv6.py crash ...")
    rc_crash, out_crash = run_host_capture(["./test-xv6.py", "crash"])

    k_sha = sha256_file(os.path.join(REPO_ROOT, "kernel", "kernel"))
    fs_sha = sha256_file(os.path.join(REPO_ROOT, "fs.img"))
    git_rev = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO_ROOT, capture_output=True, text=True).stdout.strip()

    report = [
        "=== CHỨNG NHẬN CODE FREEZE TUẦN 8 (KPROF MODULE) ===",
        f"  Git Commit       : {git_rev}",
        f"  kernel/kernel    : sha256={k_sha}",
        f"  fs.img           : sha256={fs_sha}",
        f"  Symbol Verify    : {'PASS (0 drift)' if rc_sym == 0 else 'FAIL'}",
        f"  kproftest CPUS=1 : {'15/15 PASS' if rc1 == 0 else 'FAIL'}",
        f"  kproftest CPUS=4 : {'15/15 PASS' if rc4 == 0 else 'FAIL'}",
        f"  test-xv6 crash   : {'PASS' if rc_crash == 0 else 'FAIL'}",
    ]
    report_str = "\n".join(report)
    with open(log_path, "w") as f:
        f.write(report_str + "\n\n" + out_sym + "\n" + out1 + "\n" + out4 + "\n" + out_crash)

    print("\n" + report_str)
    print(f"  Saved freeze log : {log_path}")
    if rc1 != 0 or rc4 != 0 or rc_crash != 0:
        sys.exit(1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("week", choices=["week5", "week6", "week7", "week8"])
    args = ap.parse_args()

    dispatch = {
        "week5": cmd_week5,
        "week6": cmd_week6,
        "week7": cmd_week7,
        "week8": cmd_week8,
    }
    dispatch[args.week]()


if __name__ == "__main__":
    main()
