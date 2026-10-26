# kprof — timer-driven sampling profiler (Tú)

Phạm vi: **kernel + user PC đều được lấy mẫu và phân loại**, nhưng **chỉ có PC thô**.
Chưa symbolize kernel PC (chờ symbol table của Sơn). User PC **không bao giờ** được
giải mã bằng symbol kernel.

## Đường dữ liệu

```
timer irq ─► usertrap()  ─► kprof_sample(p->trapframe->epc, 1) ─► yield()
          └► kerneltrap() ─► kprof_sample(sepc, 0)             ─► yield()
```

PC được lấy **trước** `yield()` (yield có thể gây trap lồng và đổi CSR). `kerneltrap`
dùng `sepc` đã lưu ở đầu hàm; `usertrap` dùng `trapframe->epc`.

## Cấu trúc dữ liệu và đồng bộ (`kernel/kprof.c`)

- Mỗi CPU một buffer `KPROF_PER_CPU = 256` mẫu (`struct kprof_sample`: pc, pid, cpu, is_user).
  Mỗi buffer **chỉ CPU sở hữu ghi** (trong ngắt, interrupt đã tắt) → đường timer **không lock,
  không cấp phát, không printf, không lookup symbol**; công việc O(1).
- Entry `[0, count)` hoàn chỉnh và bất biến đến khi reset. `count` được publish bằng
  `release`, đọc bằng `acquire` → reader không bao giờ thấy record ghi dở.
- Buffer đầy: không ghi tràn, tăng `dropped`. `recorded + dropped == kernel_samples + user_samples`
  (chính xác khi đã stop; khi đang chạy có thể lệch tối đa một vài mẫu giữa các CPU).
- Các lệnh điều khiển (start/stop/reset/read/status) tuần tự hóa bằng một **sleeplock**
  `kprof_ctl` (không bao giờ lấy trong ngắt) nên giữ qua `copyout` là an toàn.
- `stop`: `running = 0` rồi chờ cờ `busy` của từng CPU về 0 (seq_cst) → sau khi `stop` trả về,
  không còn CPU nào ghi → buffer ổn định.
- Không có cấp phát động ⇒ không có leak theo thiết kế; kernel stack không chứa mảng lớn.

## API: `int kprof(int cmd, void *buf, int cap)` (syscall `SYS_kprof = 26`)

Mọi lỗi trả `-1` (con trỏ sai, kích thước sai, lệnh lạ) và không panic.

| cmd | ý nghĩa | trả về |
|---|---|---|
| `START` | bắt đầu lấy mẫu; **idempotent**; không xóa dữ liệu cũ | 0 |
| `STOP` | dừng và chờ quiescent; **idempotent** | 0 |
| `RESET` | xóa toàn bộ dữ liệu; **chỉ khi đã stop**, đang chạy → `-1` (không xóa gì) | 0 / -1 |
| `STATUS` | `buf` = `struct kprof_stat`, `cap >= sizeof` | 0 / -1 |
| `READ` | `buf` = mảng `struct kprof_sample`, `cap` = số phần tử (>0). Chép `min(cap, có sẵn)` mẫu (CPU 0 trước); **cắt bớt, không tràn** | số mẫu / -1 |

`struct kprof_stat`: `version, running, ncpu, per_cpu_cap, recorded, kernel_samples,
user_samples, dropped`. `kernel_samples/user_samples` = số timer tick quan sát được khi đang
chạy (kể cả mẫu bị drop).

Snapshot không atomic toàn cục giữa các CPU; mỗi record thì hoàn chỉnh.

### Hook chỉ để test (`KPROF_TEST=1`, mặc định)

`TEST_SPIN` (vòng lặp kernel có PC biết trước, interrupt bật) và `TEST_SETCAP`
(hạ capacity để test overflow nhanh, chỉ khi đang stop). Build không hook:
`make clean && make KPROF_TEST=0 kernel/kernel fs.img` (khi đó `kproftest` các case dùng hook sẽ FAIL — chủ ý).

## Công cụ `kprof`

```
kprof start | stop | reset | status | top [N] | dump [N]
```

`top` gom theo (mode, pid, pc), sắp giảm dần. **Phần trăm = tỷ lệ trên số mẫu đã ghi
(recorded)**, không gồm mẫu bị drop, và là *ước lượng nơi timer tick rơi vào*, không phải
CPU time hay số lần gọi hàm.

## Giới hạn đã biết

- Timer ≈ 0.1 s/tick (`stimecmp + 1000000`): cần workload ≥ vài giây; vòng ngắn hơn tick có thể bị bỏ sót.
- Ngắt timer chỉ xảy ra khi interrupt bật: vùng kernel giữ spinlock/`push_off` không bao giờ được lấy mẫu
  (spin trong `acquire` cũng không thấy được → dùng `lockstat` bổ sung).
- Có sampling bias nếu workload đồng nhịp với tick.
- Process bị `yield()` mỗi tick và có thể đổi CPU, nên phân tích theo `pid`, không theo `cpu`.
- Tick được lấy mẫu trên **mọi** CPU; CPU rảnh ghi PC trong `scheduler()` với `pid = 0`.

## Kiểm thử

`user/kproftest.c` (12 case, mỗi case in PASS/FAIL), chạy bằng:

```
make kernel/kernel fs.img
./scripts/run_kprof_tests.py --cpus 4 --repeat 10   # log từng lần: results/kprof*/
```

| ID | Kiểm tra |
|---|---|
| KP01 | profiler tắt → 0 mẫu |
| KP02 | start → có mẫu, số liệu nhất quán |
| KP03 | stop → số liệu đóng băng |
| KP04 | reset khi chạy bị từ chối; sau stop xóa sạch |
| KP05 | start/stop lặp 50 lần idempotent, không đếm đôi |
| KP06 | kernel hot loop: ≥70% tick của process (theo pid) nằm trong vòng lặp |
| KP06b | user hot loop: như trên với PC user |
| KP07 | user PC < KERNBASE & pid≠0; kernel PC ≥ KERNBASE; cpu hợp lệ |
| KP08 | đọc status/snapshot liên tục khi đang lấy mẫu: không record hỏng, count không giảm |
| KP09 | buffer đầy → `dropped>0`, mỗi CPU ≤ cap |
| KP10 | con trỏ/size/lệnh sai → `-1`; truncate đúng; kernel vẫn sống |
| KP11 | mẫu gắn đúng pid của tiến trình con |

