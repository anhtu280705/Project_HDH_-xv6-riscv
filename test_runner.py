import subprocess
import select
import time
import os
import signal
import sys

# Don dep cac tien trinh qemu ton dong truoc khi chay
os.system("pkill -9 -f qemu-system-riscv64 2>/dev/null")

CPUS = 3
TIMEOUT = 25

print("[TEST RUNNER] Dang bien dich xv6...")
build = subprocess.run(["make", "-j4"], capture_output=True, text=True)
if build.returncode != 0:
    print("[FAIL] Bien dich xv6 that bai:")
    print(build.stderr)
    sys.exit(1)

print(f"[TEST RUNNER] Khoi dong QEMU (CPUS={CPUS}, timeout={TIMEOUT}s)...")
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

while time.time() - start < TIMEOUT:
    if p.poll() is not None:
        break

    r, _, _ = select.select([p.stdout], [], [], 0.2)
    if r:
        chunk = os.read(p.stdout.fileno(), 4096)
        if not chunk:
            break
        output += chunk

        if (b"$ " in output or b"init: starting sh" in output) and not sent_cmd:
            time.sleep(0.5)
            try:
                p.stdin.write(b"locktest\n")
                sent_cmd = True
            except (BrokenPipeError, OSError):
                break

        if b"=== LOCKTEST: [PASS] ===" in output:
            time.sleep(0.5)
            break

# Kill toan bo process group (bao gom ca qemu)
try:
    os.killpg(os.getpgid(p.pid), signal.SIGKILL)
except Exception:
    pass

decoded = output.decode("utf-8", errors="ignore")

print("----------------------------------------")
for line in decoded.splitlines():
    if any(k in line for k in ["lock ", "LOCKTEST", "hart", "xv6 kernel is booting"]):
        print(line)
print("----------------------------------------")

if "=== LOCKTEST: [PASS] ===" in decoded:
    print("[RESULT] Kiem thu thanh cong: PASS")
    sys.exit(0)
else:
    print("[RESULT] Kiem thu that bai hoac timeout: FAIL")
    print("\nLog chi tiet:")
    print(decoded)
    sys.exit(1)
