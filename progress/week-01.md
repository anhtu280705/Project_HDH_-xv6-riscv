# Nhật ký tiến độ — Tuần 1

## 1. Mục tiêu
Thiết lập môi trường xv6-riscv, xác minh baseline và khảo sát timer/trap
để chuẩn bị triển khai Sampling Profiler (Tú).

## 2. Môi trường
- Repository source ban đầu: mit-pdos/xv6-riscv
- Branch source ban đầu: riscv
- Commit nền: f780d7850765d74cbdcd1fff4b822d8829f7c9bd
- GNU Make: 4.4.1
- RISC-V compiler: riscv64-unknown-elf-gcc
- QEMU: 10.2.1

Chưa xác nhận tag/commit bắt buộc theo yêu cầu của giảng viên.

## 3. Kết quả đã thực hiện
- Clone source xv6-riscv.
- Build thành công kernel/kernel và fs.img.
- Khởi động xv6 bằng QEMU.
- Thử lệnh ls và echo hello trong shell xv6.
- Chạy usertests trên bản source ban đầu; kết quả cuối:
  ALL TESTS PASSED.
- Đưa source baseline vào repository cá nhân, nhánh feat/tu.

Ghi chú: build và usertests được thực hiện trên bản clone source ban đầu
trước khi sao chép vào repository cá nhân. Chưa sửa code profiler.

## 4. Khảo sát timer/trap
- start.c: timerinit() bật cấu hình SSTC và đặt stimecmp.
- trap.c: devintr() nhận diện supervisor timer interrupt và gọi clockintr().
- trap.c: usertrap() lưu r_sepc() vào p->trapframe->epc.
- trap.c: prepare_return() ghi p->trapframe->epc trở lại sepc.
- trap.c: kerneltrap() lưu sepc, sstatus và scause ở đầu hàm;
  khôi phục sepc và sstatus sau xử lý ngắt.
- kernelvec.S: lưu các thanh ghi tổng quát trên kernel stack,
  sau đó gọi kerneltrap().
- trampoline.S: uservec lưu thanh ghi user vào TRAPFRAME,
  chuyển sang kernel page table rồi gọi usertrap().

## 5. Việc chưa hoàn thành
- Chưa xác nhận tag môn học yêu cầu.
- Chưa chốt phạm vi kernel/user của profiler.
- Chưa triển khai sampling counter hoặc kprof.
- Chưa chạy benchmark overhead.
- Chưa có kết quả kiểm thử profiler.

## 6. Công việc tiếp theo
- Chốt phạm vi PC được lấy mẫu và cách phân loại user/kernel.
- Thiết kế sampling counter hữu hạn, an toàn khi nhiều CPU hoạt động.
- Làm prototype tối giản và xây dựng cách kiểm chứng số mẫu.
