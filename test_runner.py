import subprocess
import select
import time
import os
import signal
import sys

CPUS = 4
DEFAULT_TIMEOUT = 25

def kill_qemu():
    os.system("pkill -9 -f qemu-system-riscv64 2>/dev/null")

def run_test_case(name, cmd, pass_marker, expect_panic=False, timeout=DEFAULT_TIMEOUT):
    kill_qemu()
    print(f"[RUNNER] Dang chay '{name}' (cmd='{cmd}', expect_panic={expect_panic})...")
    p = subprocess.Popen(
        ["make", "qemu", f"CPUS={CPUS}"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        preexec_fn=os.setsid,
        bufsize=0
    )

    output = b""
    start = time.time()
    sent_cmd = False
    timed_out = False

    while True:
        if time.time() - start >= timeout:
            timed_out = True
            break

        if p.poll() is not None:
            break

        r, _, _ = select.select([p.stdout], [], [], 0.2)
        if r:
            chunk = os.read(p.stdout.fileno(), 4096)
            if not chunk:
                break
            output += chunk

            if (b"$ " in output or b"init: starting sh" in output) and not sent_cmd:
                time.sleep(0.4)
                try:
                    p.stdin.write((cmd + "\n").encode("utf-8"))
                    sent_cmd = True
                except (BrokenPipeError, OSError):
                    break

            if (not expect_panic and pass_marker.encode("utf-8") in output) or (expect_panic and b"exiting" in output):
                time.sleep(0.3)
                break

            if not expect_panic and b"panic:" in output:
                time.sleep(0.2)
                break

    try:
        os.killpg(os.getpgid(p.pid), signal.SIGKILL)
    except Exception:
        pass
    kill_qemu()

    decoded = output.decode("utf-8", errors="ignore")

    if timed_out:
        print(f"  -> [FAIL] {name}: TIMEOUT sau {timeout}s")
        return False, decoded

    if expect_panic:
        if ("panic" in decoded or "exiting" in decoded) and pass_marker in decoded:
            print(f"  -> [PASS] {name}: Bat duoc expected panic & tu dong reboot QEMU")
            return True, decoded
        else:
            print(f"  -> [FAIL] {name}: Khong thay panic nhu ky vong")
            return False, decoded
    else:
        if "exiting" in decoded:
            print(f"  -> [FAIL] {name}: Kernel bi unexpected PANIC!")
            return False, decoded
        if pass_marker in decoded:
            print(f"  -> [PASS] {name}")
            return True, decoded
        print(f"  -> [FAIL] {name}: Khong tim thay '{pass_marker}'")
        return False, decoded

def main():
    print("[TEST RUNNER] Dang bien dich xv6...")
    build = subprocess.run(["make", "-j4"], capture_output=True, text=True)
    if build.returncode != 0:
        print("[FAIL] Bien dich xv6 that bai:")
        print(build.stderr)
        sys.exit(1)

    # Danh sach cac bai test tuan tu (co khoi dong lai QEMU giua cac case)
    test_suite = [
        ("Lockstat Oracles", "locktest", "=== LOCKTEST: [PASS] ===", False, 25),
        ("Lockstat CLI Sort", "lockstat", "LOCK NAME", False, 20),
        ("Lockstat CLI Reset", "lockstat reset", "counters reset to 0", False, 20),
        ("Lockstat CLI Delta", "lockstat delta 1", "=== LOCKSTAT DELTA", False, 20),
        # Demo co che Expected-Panic Reboot (san sang cho module Backtrace cua Son)
        ("Expected-Panic Reboot Check", "echo SIMULATE_PANIC; kill 1", "panic: init exiting", True, 20),
    ]

    passed = 0
    for name, cmd, marker, exp_panic, t_out in test_suite:
        ok, log = run_test_case(name, cmd, marker, exp_panic, t_out)
        if ok:
            passed += 1
        else:
            print("\n--- LOG LOI ---")
            print(log[-1500:])
            print("---------------\n")

    print("========================================")
    print(f"[SUMMARY] Ket qua kiem thu: {passed}/{len(test_suite)} PASS")
    print("========================================")
    sys.exit(0 if passed == len(test_suite) else 1)

if __name__ == "__main__":
    main()
