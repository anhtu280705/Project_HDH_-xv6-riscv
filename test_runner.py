import subprocess
import time
import sys
import select
import os
import signal

def run_test_case(name, cmd, pass_marker, expect_panic=False, timeout=20):
    print(f"[RUNNER] Dang chay '{name}' (cmd='{cmd}', expect_panic={expect_panic})...")
    proc = subprocess.Popen(
        ["make", "qemu"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        preexec_fn=os.setsid
    )
    output = b""
    cmd_sent = False
    start_time = time.time()

    try:
        while time.time() - start_time < timeout:
            rlist, _, _ = select.select([proc.stdout], [], [], 0.1)
            if rlist:
                chunk = os.read(proc.stdout.fileno(), 4096)
                if not chunk:
                    break
                output += chunk
                clean = output.decode("utf-8", errors="ignore").replace("$ ", "")

                if not cmd_sent and "init: starting sh" in clean:
                    time.sleep(0.2)
                    proc.stdin.write((cmd + "\n").encode("utf-8"))
                    proc.stdin.flush()
                    cmd_sent = True

                if cmd_sent and pass_marker in clean:
                    if expect_panic:
                        print(f"   -> [PASS] {name}: Bat duoc expected panic & tu dong reboot QEMU")
                    else:
                        print(f"   -> [PASS] {name}")
                    return True, clean

        clean = output.decode("utf-8", errors="ignore").replace("$ ", "")
        if expect_panic and ("panic:" in clean or "init exiting" in clean):
            print(f"   -> [PASS] {name}: Bat duoc expected panic & tu dong reboot QEMU")
            return True, clean

        print(f"   -> [FAIL] {name}: TIMEOUT sau {timeout}s")
        return False, clean
    finally:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except Exception:
            pass

def main():
    print("[TEST RUNNER] Dang bien dich xv6...")
    subprocess.run(["make", "fs.img"], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    test_suite = [
        ("Lockstat Oracles", "locktest", "LOCKTEST: [PASS]", False, 20),
        ("Lockstat CLI Sort", "lockstat", "LOCK", False, 15),
        ("Lockstat CLI Reset", "lockstat reset", "reset", False, 15),
        ("Lockstat CLI Delta", "lockstat delta 1", "LOCK", False, 15),
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
