# Hướng dẫn AI làm việc với dự án xv6 observability

## 1. Context và mục tiêu

Dự án môn Hệ điều hành: **Đề tài 3 — Kernel Debugging and Observability Toolkit (★★)** trên xv6-riscv. Nhóm gồm **Tú, Sơn, Việt**.

Sản phẩm bắt buộc:

1. Frame-pointer backtrace với symbol table nhúng trong kernel; in backtrace khi panic và kiểm chứng bằng `addr2line`.
2. Timer-driven sampling profiler, công cụ user-space `kprof`, kiểm thử phát hiện hot loop.
3. Bộ đếm acquisition và contention theo từng lock, công cụ `lockstat`; demo lock contention khi chạy `usertests`.

Mục tiêu: code đúng, nhỏ gọn, giải thích được, ổn định khi chạy nhiều CPU và có kiểm thử/đo lường tái lập. Giữ hành vi xv6 hiện có. Mỗi thành viên phải có đóng góp kernel thực chất và hiểu code mình nộp.

`lockdep-lite` là stretch, không thuộc core. Không tự thêm stretch hoặc mở rộng sang scheduler, memory allocator, filesystem hay user-space symbolization nếu task không yêu cầu. Sửa các subsystem này ở mức tối thiểu khi core thực sự cần và giải thích lý do.

File này mô tả quy tắc và yêu cầu, **không chứng minh tính năng đã tồn tại hoặc test đã pass**. Trạng thái phải được xác minh từ repository.

## 2. Ngôn ngữ và cách làm việc

- Trao đổi, giải thích và báo cáo với nhóm bằng tiếng Việt Unicode. Tên định danh dùng tiếng Anh; comment theo quy ước hiện có của repository.
- Làm task được giao đến kết quả có thể review; không chỉ đưa kế hoạch khi người dùng yêu cầu triển khai.
- Tự giải quyết lựa chọn cục bộ, có thể đảo ngược. Chỉ hỏi khi thiếu thông tin ảnh hưởng correctness, yêu cầu môn học, ABI dùng chung hoặc phạm vi đáng kể.
- Không suy đoán mức kinh nghiệm hay gán chức danh cho thành viên.
- Không thêm dependency, framework, kiến trúc phức tạp khi xv6 và toolchain hiện có đã đáp ứng.
- Không lấy code lời giải công khai làm phần tự triển khai. Tài liệu/code tái sử dụng phải ghi nguồn và tuân thủ chính sách môn học. Giải thích thay đổi đủ để thành viên tự bảo vệ trong Q&A.

## 3. Bắt đầu mỗi task

Trước khi sửa code:

1. Đọc `AGENTS.md` ở phạm vi áp dụng, `README`, Makefile và tài liệu thiết kế/API/handoff hiện có. Chỉ dẫn ở thư mục con áp dụng cho phạm vi tương ứng.
2. Kiểm tra branch, commit, trạng thái worktree và diff liên quan. Không ghi đè thay đổi của người khác; không reset, clean hoặc checkout để xóa thay đổi.
3. Xác định base tag do giảng viên quy định từ bằng chứng trong repo hoặc thông tin người dùng. Không tự nâng cấp xv6 lên nhánh mới nhất. Nếu chưa biết tag, vẫn có thể khảo sát nhưng không tuyên bố đã đúng môi trường môn học.
4. Đọc đường thực thi thật trước khi thiết kế: trap/timer, ABI RISC-V, compiler flags, stack, spinlock, syscall và build/link liên quan. Không áp dụng code theo trí nhớ của phiên bản xv6 khác.
5. Xác định mục tiêu task, dependency, invariants, acceptance criteria và test nhỏ nhất có thể kiểm chứng kết quả.
6. Dùng lệnh build/test được repository hỗ trợ. Không giả định `make test`, `kprof`, `lockstat` hay script nào đã có.

Nếu chưa có môi trường chạy, xác minh công cụ còn thiếu và báo cụ thể; không tuyên bố kiểm chứng runtime bằng việc chỉ đọc code hoặc compile. Không tự cài công cụ hệ thống khi chưa được yêu cầu/cho phép.

## 4. Phân công và phối hợp

| Thành viên | Module chính | Phần liên quan |
|---|---|---|
| Tú | Sampling profiler, `kprof` | Timer hooks, điều khiển/đọc sample, thiết kế phép đo overhead, tích hợp symbol |
| Sơn | Backtrace, embedded symbol table | Stack walker, panic integration, symbol generation, clean build và regression |
| Việt | Lock statistics, `lockstat` | Counter/registry/snapshot, test runner, CI QEMU và stress automation |

- Cả ba viết test và tài liệu module mình. Không đẩy toàn bộ kiểm thử hoặc sửa lỗi tích hợp cho người duy trì runner.
- Review mặc định: Sơn review Tú, Việt review Sơn, Tú review Việt. AI review không được ghi nhận thay cho review của thành viên nếu chưa có xác nhận.
- File dùng chung cần phối hợp: Sơn phụ trách chính Makefile/linker; Tú trap hooks; Việt spinlock instrumentation. Quyền sở hữu là để phối hợp, không phải cấm sửa các file cần thiết cho task.
- Trước khi đổi ABI hoặc cấu trúc dùng chung, đọc tất cả caller và test bị ảnh hưởng; cập nhật chúng cùng tài liệu trong một thay đổi nhất quán.
- Không tự đổi phân công hoặc tạo commit/PR/push/message ra bên ngoài chỉ vì file này đề cập quy trình. Thực hiện theo yêu cầu hiện tại của người dùng.
- Không tự khởi chạy nhiều agent nếu người dùng chưa yêu cầu. Khi được phép, chỉ chia task độc lập và chỉ rõ vùng file để tránh ghi đè.

## 5. Quy tắc sửa code

- Ưu tiên patch nhỏ, mỗi patch giải quyết một vấn đề; tránh refactor/format toàn repository.
- Theo phong cách C hiện có của xv6. Không dùng API libc/host trong kernel hoặc giả định đầy đủ POSIX ở user-space xv6.
- Kiểm tra tràn số, độ dài buffer, con trỏ user, mã lỗi và cleanup trên mọi nhánh thất bại.
- Comment giải thích invariant, ownership, lock order hoặc lựa chọn khó; không viết lại điều hiển nhiên từ code.
- Không để TODO thay thế một yêu cầu core rồi báo hoàn thành.
- Không vô hiệu hóa assert, bỏ test hoặc nới ngưỡng chỉ để suite xanh. Nếu test sai, giải thích oracle sai ở đâu và sửa có căn cứ.
- Debug instrumentation/test hooks phải có phạm vi rõ và được kiểm soát; không để syscall gây panic tùy ý xuất hiện ngoài thiết kế đã thống nhất.
- Giữ đúng kiến trúc RISC-V và semantics của các primitive sẵn có. Không thay đổi memory ordering theo phỏng đoán.

## 6. Các invariant kernel bắt buộc

### 6.1. Locking và interrupt

- Xác định context của hàm: user syscall, kernel trap, interrupt, boot hoặc panic; không mặc định luôn có process hiện tại.
- Không sleep/yield hoặc thực hiện I/O có thể block khi đang giữ spinlock hay ở interrupt context.
- Trong đường timer lấy mẫu: không cấp phát, không printf, không filesystem I/O và không symbol lookup tốn thời gian. Công việc phải có giới hạn rõ.
- Kiểm tra lock order và balanced acquire/release trên mọi nhánh lỗi. Giữ nguyên semantics `push_off/pop_off` theo tag hiện tại.
- Không thêm global lock vào instrumentation nếu nó có thể tự instrument, tạo recursion/deadlock hoặc trở thành nút nghẽn.
- Nêu rõ cơ chế đồng bộ cho counters, snapshot, reset và lifecycle; `volatile` không thay thế atomic hay locking.
- Khi copy dữ liệu ra user, ưu tiên chụp snapshot có giới hạn trước, thả lock rồi `copyout`; xác minh độ an toàn với implementation thực tế. Không giữ khóa đo lường qua thao tác có thể tái nhập đường instrument.

### 6.2. Bộ nhớ và lifetime

- Mọi bảng/buffer có capacity, kiểm tra bounds và cách xử lý đầy bảng rõ ràng.
- Không ghi vượt buffer; dropped/overflow phải quan sát được thay vì âm thầm làm hỏng dữ liệu.
- Kernel stack nhỏ: không đặt snapshot/array lớn trên stack; xác định nơi lưu và bảo vệ truy cập đồng thời.
- Không xuất raw kernel pointer trong ABI người dùng khi không cần. Địa chỉ PC phục vụ chẩn đoán phải được coi là dữ liệu có chủ đích trong thiết kế.
- Không lưu pointer tới process/lock/object vượt lifetime. Có chính sách register/unregister hoặc chứng minh object sống suốt thời gian sử dụng.
- Nếu cấp phát tài nguyên, có cleanup cho success, error, exit và reset phù hợp.

## 7. Yêu cầu riêng của backtrace và symbol table

- Xác minh frame-pointer compiler flags, ABI, stack layout, inlining và tail calls trên tag thực tế.
- Trước mỗi dereference, kiểm tra alignment, stack bounds và arithmetic overflow. Duyệt hữu hạn; dừng khi frame không tiến triển hoặc vượt depth limit.
- Không giả định `myproc()` khác null khi panic. Với boot/interrupt stack chưa hỗ trợ, dừng an toàn và ghi rõ giới hạn.
- Bảo vệ panic path khỏi recursive panic hoặc phụ thuộc lock đang hỏng/bị giữ; tận dụng cơ chế in panic hiện có, không tùy tiện thêm lock.
- Backtrace chỉ chứng minh chuỗi frame được hỗ trợ ở stack hiện tại, không tự khẳng định xuyên user/kernel trap.
- Symbol table phải khớp **ELF cuối cùng chạy trong QEMU**. Nhúng symbol có thể thay đổi layout giữa các bước link; phải có kiểm tra tự động phát hiện mismatch.
- Dùng symbol phù hợp, thứ tự địa chỉ và cách xử lý duplicate/unknown rõ ràng. Không gán tên hàm gần nhất cho địa chỉ ngoài vùng hợp lệ.
- Lookup phải an toàn ở panic context, không cấp phát và không truy cập disk.
- Đối chiếu `addr2line` đúng toolchain/ELF. Phân biệt return address với call site; không trừ cố định 4 byte nếu chưa chứng minh phù hợp với mã RISC-V thực tế.
- Test call chain cần kiểm soát compiler optimization, ví dụ `noinline`; không tắt tối ưu toàn kernel chỉ để test đẹp.

## 8. Yêu cầu riêng của sampling profiler

- Xác định PC của ngữ cảnh bị ngắt trước khi CSR có thể bị thay đổi. Phân biệt kernel PC và user PC theo trap path thật.
- **Không giải mã user PC bằng symbol table kernel.** Muốn có tên hàm user phải có metadata đúng executable/address space và phạm vi được chấp thuận.
- Trước implementation lớn, chốt phạm vi profiler: kernel-only hay gồm user. Nếu kernel-only, vẫn phải thiết kế test hot loop có thể bị timer ngắt và giải thích giới hạn demo CPU-heavy.
- Ưu tiên thu mẫu theo CPU hoặc giải pháp giảm contention; chứng minh an toàn migration/preemption và khả năng đọc snapshot.
- Dữ liệu có capacity, số mẫu hợp lệ, dropped/overflow và metadata cần thiết để diễn giải.
- State contract cho start/stop/reset/read phải rõ: hành vi gọi lặp, reset khi đang chạy, stop nhiều CPU và snapshot đang chạy. Không tự thêm nhiều syscall trước khi chốt API nhỏ nhất.
- Tỷ lệ mẫu là ước lượng, không phải số lần gọi hàm hoặc CPU time tuyệt đối. Nêu mẫu số của phần trăm.
- Timer sampling có bias khi interrupts bị tắt, workload quá ngắn hoặc nhịp workload trùng nhịp lấy mẫu. Không hứa quan sát được mọi vòng lặp kernel.
- Chuyển aggregation/symbolization ra ngoài đường interrupt nếu có thể. `kprof` phải báo unknown/dropped rõ và xử lý trạng thái chưa có mẫu.

## 9. Yêu cầu riêng của lock statistics

- Chốt phạm vi spinlock; không tuyên bố đã đo sleep lock nếu chưa instrument và test.
- Định nghĩa metric nhất quán:
  - `acquisitions`: số lần lấy lock thành công.
  - `contended_acquisitions`: số lần lấy lock có ít nhất một lần thử thất bại.
  - `failed_attempts`: tổng số lần thử lấy lock thất bại.
- Không gọi failed attempts là thời gian chờ hoặc CPU cycles. Đo thời gian thật cần cơ chế riêng và phân tích overhead.
- Cập nhật counter không race; không dùng counter thường ở đường nhiều CPU cùng ghi nếu không có đồng bộ phù hợp.
- Lock name không phải ID duy nhất; nhiều instance có thể cùng tên. Registry có giới hạn và xử lý lifetime/overflow.
- Snapshot có thể là xấp xỉ giữa các record, nhưng phải nêu consistency contract; không tuyên bố snapshot toàn cục atomic nếu không thực hiện.
- Reset cạnh tranh với update phải có semantics và kiểm thử; có thể dùng baseline/delta nếu phù hợp thiết kế thay vì ghi 0 thiếu đồng bộ.
- Không giữ lock thống kê rồi gọi một đường làm tăng lại thống kê theo cách gây recursion.
- Contention test CPUS=4 cần tạo overlap có kiểm soát. CPUS=1 không bắt buộc có spin contention >0.

## 10. Build và kiểm thử

### 10.1. Quy trình cho một patch

1. Build với lệnh và toolchain của repo.
2. Chạy test trực tiếp cho hành vi vừa thay đổi, bao gồm failure path liên quan.
3. Với thay đổi trap/locking/lifetime, chạy kiểm tra CPUS=1 và CPUS=4 tương ứng.
4. Với ABI/build dùng chung, kiểm tra caller và clean build liên quan.
5. Chạy regression phù hợp rủi ro. Trước milestone/final phải có full `usertests` và `grind` theo kế hoạch, không chỉ targeted tests.

Không chạy lại toàn bộ stress dài sau mỗi sửa tài liệu. Không lặp lại test đã pass nếu không có thay đổi hoặc nghi vấn mới. Không bỏ kiểm chứng cần thiết để tiết kiệm thời gian.

### 10.2. Chuẩn suite và milestone

- Suite chạy bằng một lệnh, mỗi case kiểm tra kết quả và in PASS/FAIL. Host runner có timeout và exit status cho CI.
- Ít nhất 25 case có ý nghĩa để hướng tới mức Excellent; không đếm nhiều lần lặp cùng case thành các test độc lập.
- Bao phủ backtrace correctness/bad frame, symbol mismatch, profiler state/overflow/user buffer/hot loop, lock counters/concurrency/registry, integration và leak.
- Stress CPUS=1 và CPUS=4, mỗi cấu hình lặp ít nhất 10 lần; lưu kết quả từng lần. Đây là chuẩn nghiệm thu suite/milestone, không yêu cầu mọi patch nhỏ chạy toàn bộ.
- `usertests` và `grind` phải giữ hoạt động; report ghi lệnh, thời lượng nếu grind không tự kết thúc, timeout và lỗi.
- Leak checks gồm free-page và free-inode trước/sau cleanup. Kiểm soát hoạt động nền và allowance có giải thích; không cộng allowance tùy ý để che leak.
- Expected-panic test dùng QEMU riêng/reboot hoặc harness rõ ràng; phân biệt panic mong đợi với kernel crash bất ngờ.
- Test thống kê có workload đủ dài và ngưỡng có căn cứ; tránh assert phần trăm tuyệt đối gây flaky. Không retry đến khi pass rồi giấu các lần fail.
- Lưu commit, cấu hình CPU/RAM/toolchain, lệnh và raw log. Không bịa log, số liệu hoặc kết quả pass.

### 10.3. CI

- Việt duy trì runner/CI, mỗi module cung cấp test tự động tương thích.
- Thiết lập CI build/boot và test đầu tiên trước checkpoint tuần 5.
- Timeout phải phân biệt QEMU treo với workload chậm; giữ console log khi fail.
- Không ghi token/secret vào repository hoặc log. Không tự sửa cấu hình tài khoản/quyền bên ngoài phạm vi được yêu cầu.

## 11. Measurement và evaluation

- Baseline là stock xv6 đúng tag; ghi commit và compiler/QEMU configuration.
- Phân biệt stock, toolkit với profiler tắt và toolkit với profiler bật. Profiler tắt không đồng nghĩa toàn bộ instrumentation tắt nếu lock counters vẫn chạy.
- Nếu đo riêng lock instrumentation, dùng cấu hình bật/tắt có kiểm soát và ghi khác biệt build.
- Ít nhất 5 lần đo cho mỗi điểm dữ liệu; báo trung bình và độ phân tán, giữ raw data. Một điểm là tổ hợp workload/config/CPUS cần so sánh.
- Cùng môi trường, kích thước workload, RAM, CPU và image phù hợp; ghi host noise và giới hạn timer resolution. Không so hai cấu hình khác nhau rồi gán toàn bộ chênh lệch cho toolkit.
- Nếu công bố overhead phần trăm: `(thời gian toolkit - thời gian baseline) / thời gian baseline × 100`, dùng phép đo cùng loại và diễn giải độ nhiễu.
- Không đặt kết luận “nhanh hơn”, “overhead thấp” hay “không ảnh hưởng” chỉ từ một lần chạy.
- Biểu đồ phải truy ngược được về raw data và script; không dùng dữ liệu minh họa làm kết quả thực nghiệm.

## 12. Lịch và ưu tiên

- Tuần 1–2: baseline, prototype ba module, chốt API và phạm vi.
- Tuần 3: proposal 2–3 trang có cấu trúc dữ liệu, file sửa, syscall và test plan.
- Tuần 5: checkpoint 10 phút, tests trong CI; mục tiêu nội bộ là ba module end-to-end và ít nhất 15 case.
- Tuần 6–7: 25+ test, stress, leak/regression, benchmark và báo cáo.
- Tuần 8: clean build/test từ final tag, code freeze.
- Tuần 9: demo 20 phút gồm Q&A, báo cáo 15–20 trang và contribution statements.

Đây là tuần môn học; chưa có ngày cụ thể trong file. Không tự đặt deadline lịch hoặc tuyên bố đang ở tuần nào. Báo cáo tuần và tracker phải phản ánh công việc thật.

Nếu trễ tiến độ, ưu tiên core correctness, regression và bằng chứng test; bỏ stretch trước. Không tự cắt một tính năng core để tuyên bố đề tài hoàn thành.

Mâu thuẫn tài liệu cần giảng viên xác nhận: giới thiệu rubric ghi Testing 20 điểm, bảng chi tiết ghi 5; phần practices ghi CI optional nhưng checkpoint yêu cầu first tests in CI. Nhóm chọn có CI trước checkpoint. Không tự diễn giải điểm chấm thành quy định đã xác nhận.

## 13. Tài liệu và bàn giao phiên làm việc

Tận dụng cấu trúc repo đang có. Nếu chưa có, khi task cần có thể dùng:

- `docs/design.md`: kiến trúc, invariants, kernel/user scope, limitations.
- `docs/api.md`: ABI, record fields, errors và state/snapshot/reset semantics.
- `docs/decisions.md`: quyết định quan trọng, lý do và ảnh hưởng.
- `docs/handoff.md`: trạng thái thực tế để người hoặc AI khác tiếp tục.
- `tests/`, `scripts/`, `results/`: tests, automation và dữ liệu tái lập.

Không tạo nhiều tài liệu rỗng hoặc cập nhật tài liệu không liên quan. Khi task thay đổi code/API, cập nhật tài liệu liên quan và handoff ngắn nếu repo dùng handoff.

Kết thúc task, báo:

1. Đã thay đổi gì và giải quyết vấn đề nào.
2. File chính và invariants cần hiểu.
3. Lệnh kiểm thử đã chạy, kết quả thật và cấu hình.
4. Những gì chưa kiểm chứng, lỗi/blocker còn lại.
5. Dependency với module khác và bước tiếp theo cụ thể nếu còn việc.

Phân biệt rõ “đã code”, “đã build”, “đã test” và “đạt nghiệm thu”. Không ghi task Done khi mới có prototype hoặc chưa đáp ứng acceptance criteria.

Mẫu handoff:

```text
Thành viên / task / nhánh / commit:
Context và mục tiêu:
Thay đổi đã làm:
API hoặc invariant thay đổi:
Lệnh test, cấu hình, kết quả thật:
Chưa kiểm chứng / lỗi còn lại:
Ảnh hưởng Tú / Sơn / Việt:
Bước tiếp theo và tiêu chí hoàn thành:
```

## 14. Điều kiện hoàn thành

Một task hoàn thành khi đáp ứng scope được giao, có kiểm chứng tương ứng rủi ro, không phá phần khác, tài liệu/API nhất quán và người thực hiện hiểu được code.

Đồ án hoàn thành khi cả ba core feature hoạt động, công cụ dùng được, test/stress/leak/regression đạt, benchmark có dữ liệu thật, clean final build tái lập và báo cáo/demo khớp code. File này không thay thế xác nhận chấm điểm của giảng viên.
