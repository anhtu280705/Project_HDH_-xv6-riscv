# Tổng quan và phân công đề tài 3 xv6-riscv

Ngày lập: 13/10/2026. Thành viên: Tú, Sơn, Việt.

Tên đề tài: **Kernel Debugging and Observability Toolkit**, mức độ ★★.

Tài liệu này là ngữ cảnh dùng chung cho cả nhóm và cho AI hỗ trợ từng thành viên. Nó mô tả yêu cầu, phương án triển khai dự kiến, phân công, kế hoạch và tiêu chí nghiệm thu. Đây là kế hoạch chưa triển khai: chưa có repository, tag môn học, kết quả build hoặc kết quả test được xác minh trong tài liệu này.

## 1. Context và mục tiêu của đồ án

Nhóm phát triển thêm bộ công cụ quan sát và gỡ lỗi bên trong kernel xv6-riscv. Hệ thống chạy bằng QEMU với RISC-V GNU toolchain, dựa trên đúng tag xv6 do giảng viên quy định.

Sản phẩm cần giúp người sử dụng trả lời ba câu hỏi:

1. Khi kernel panic, chuỗi hàm dẫn đến lỗi là gì?
2. Khi chạy workload nặng CPU, thời gian lấy mẫu tập trung ở những địa chỉ hoặc hàm nào?
3. Trong lúc nhiều CPU cùng hoạt động, lock nào được lấy nhiều và bị tranh chấp nhiều?

Ba phần tương ứng là symbolized backtrace, timer-driven sampling profiler và per-lock statistics. Nhóm phải triển khai kernel code, công cụ người dùng, bộ kiểm thử, benchmark và tài liệu. Không cần làm giao diện đồ họa.

Mục tiêu chung: hoàn thành ba chức năng core, hoạt động ổn định trên CPUS=1 và CPUS=4, có bằng chứng kiểm thử lặp lại được, đo overhead và giải thích được thiết kế trong vấn đáp. Không đặt chỉ tiêu điểm số chưa có căn cứ.

## 2. Yêu cầu từ tài liệu môn học

Nguồn đã đối chiếu: “Xv6-riscv Term Projects.docx” và hàng số 3 trong “Project catalogue.docx”. Những kiến trúc, tên API, lịch công việc và test bên dưới là đề xuất của nhóm; không phải tất cả đều là yêu cầu nguyên văn của giảng viên.

### 2.1. Phạm vi bắt buộc của đề tài 3

| Thành phần | Core theo catalogue | Sản phẩm và minh chứng |
|---|---|---|
| Backtrace | Lần theo frame pointer, có symbol table nhúng trong kernel | Backtrace khi panic; kiểm thử độ chính xác đối chiếu `addr2line` |
| Profiler | Lấy mẫu dựa trên timer | Công cụ `kprof`; kiểm thử phát hiện hot loop; demo workload nặng CPU |
| Lock statistics | Bộ đếm acquisition và contention cho từng lock | Công cụ `lockstat`; demo các lock tranh chấp nhiều khi chạy `usertests` |

Stretch được catalogue nêu: **lockdep-lite phát hiện chu trình phụ thuộc lock**. Chỉ cân nhắc sau khi core, test và measurement đã đạt mục tiêu. Bỏ stretch không có nghĩa là thiếu core, nhưng catalogue nói stretch cần thiết cho mức điểm cao nhất; cần hỏi giảng viên cách áp dụng cụ thể, không tự hứa hoàn thành core sẽ đạt tối đa.

### 2.2. Quy định chung cần đưa vào kế hoạch

- Mỗi người có đóng góp kernel code đáng kể, kèm lịch sử commit và mô tả đóng góp.
- Repository private, chia sẻ quyền đọc với giảng viên; từ tuần 2 giảng viên có quyền đọc tracker, sơ đồ và repository.
- Build bằng một lệnh; test suite chạy bằng một lệnh, từng case tự kiểm tra kết quả và in PASS/FAIL.
- Mục tiêu mức Excellent của Testing: ít nhất 25 test có ý nghĩa, có stress và failure path, chạy lặp lại được.
- Stress/concurrency trên CPUS=1 và CPUS=4, lặp ít nhất 10 lần; lưu log từng lần.
- Kiểm tra free-page và free-inode trước/sau suite; phải kiểm soát tài nguyên nền và dọn dữ liệu test trước khi so sánh.
- `usertests` và `grind` tiếp tục hoạt động; thay đổi semantics phải giải trình.
- Benchmark ít nhất 5 lần cho mỗi điểm dữ liệu; báo cáo trung bình và độ phân tán.
- Báo cáo tuần 1 trang từ đăng ký đến code freeze: công việc từng người, commit/test, so sánh TODO tuần trước, vấn đề và TODO tuần sau.
- Tracker bắt buộc: Jira, hoặc Trello/GitHub Projects nếu được chấp thuận. Sơ đồ thiết kế bắt buộc: Figma/FigJam hoặc công cụ tương đương.
- Scrum sprint 1 tuần là khuyến nghị; PR review chéo được khuyến khích. Nhóm chọn áp dụng cả hai.
- Tài liệu ghi CI là optional ở phần practices nhưng checkpoint tuần 5 yêu cầu first tests in CI. Nhóm lên kế hoạch có CI trước tuần 5 để đáp ứng cả hai cách diễn đạt.
- Tuần 3: proposal 2–3 trang. Tuần 5: checkpoint, demo 10 phút. Tuần 8: tag `final`. Tuần 9: demo 20 phút gồm Q&A, báo cáo 15–20 trang và bản đóng góp cá nhân.
- Phần code freeze có liệt kê report, trong khi mốc tuần 9 ghi nộp report: nhóm chuẩn bị bản đầy đủ ở tuần 8 và hỏi giảng viên về hạn nộp chính thức.
- Không build được final tag bị trừ 20 điểm. AI chỉ được dùng theo chính sách môn học; mỗi người phải hiểu và giải thích được code mình nộp. Code tái sử dụng cần dẫn nguồn và không được chấm như phần tự làm.

### 2.3. Rubric để kiểm tra tiến độ

| Tiêu chí trong bảng chi tiết | Điểm | Minh chứng nhóm phải có |
|---|---:|---|
| Core functionality | 30 | Ba module chạy đúng, edge cases, regression |
| Final demo và Q&A | 20 | Demo tái hiện được; cả ba giải thích code |
| Code quality và kernel engineering | 10 | Locking, lifetime, không leak, invariant rõ |
| Progress reports | 10 | Báo cáo tuần và đóng góp cá nhân |
| Usage of project tools | 10 | Tracker, sơ đồ, công việc có người phụ trách |
| Testing | 5 | Suite tự động, 25+ case, stress và lỗi đầu vào |
| Measurement và evaluation | 5 | Baseline, chạy lặp, biểu đồ, diễn giải đúng |
| Proposal và midterm | 5 | Thiết kế khả thi và checkpoint |
| Final report | 5 | Thiết kế, kết quả, hạn chế, phân công |

Mâu thuẫn cần hỏi giảng viên: phần giới thiệu ghi Testing 20 điểm, bảng chi tiết ghi 5. Bảng trên chỉ phản ánh bảng chi tiết, không giải quyết thay giảng viên. Đề tài ★★ có bonus +3 khi Core ít nhất Satisfactory, tổng điểm nhóm tối đa 100. Điểm cá nhân nhân hệ số 0,7–1,1; không giải thích được code của mình thì hệ số tối đa 0,8.

## 3. Phân công tổng thể

| Thành viên | Phần chính | Kết quả phải bàn giao | Công việc chung phụ trách |
|---|---|---|---|
| Tú | Sampling profiler và `kprof` | Hook timer đúng ngữ cảnh, thu thập mẫu, start/stop/reset/read, công cụ hiển thị và test profiler | Thiết kế benchmark, phân tích overhead, tích hợp đường dữ liệu profiler–symbol |
| Sơn | Backtrace và symbol table | Stack walker an toàn, symbol lookup, nhúng symbol lúc build, panic backtrace, test với `addr2line` | Build tái lập, regression `usertests/grind`, tổng hợp sơ đồ kiến trúc |
| Việt | Lock statistics và `lockstat` | Counters từng lock, snapshot/reset an toàn, công cụ hiển thị và contention tests | Test runner một lệnh, CI QEMU, tự động hóa stress và demo |

Cả ba cùng chốt API, review code, viết test module mình và cập nhật tracker. Mỗi người viết phần báo cáo và hướng dẫn sử dụng của module mình. Tổng hợp báo cáo tuần luân phiên: tuần 1 Tú, tuần 2 Sơn, tuần 3 Việt, sau đó lặp lại. Người tổng hợp không viết thay phần đóng góp của người khác.

Review mặc định: Sơn review Tú; Việt review Sơn; Tú review Việt. Thay đổi dùng chung ở trap, syscall, locking hoặc Makefile cần người bị ảnh hưởng cùng xem trước merge. Người sở hữu module chịu trách nhiệm sửa lỗi module đó sau tích hợp.

## 4. Định hướng kỹ thuật và các quyết định phải chốt

Đây là thiết kế dự kiến. Trước khi viết code, đọc repository thực tế và `AGENTS.md` nếu có; kiểm tra compiler flags, timer/trap path, stack layout và API syscall của tag môn học. Không áp dụng máy móc mã của một phiên bản xv6 khác.

### 4.1. Backtrace và symbol table — Sơn

Context: backtrace lần theo các frame của kernel stack hiện tại, lấy return address rồi ánh xạ sang tên hàm. Symbol table phải nằm sẵn trong bộ nhớ để panic không phải đọc filesystem.

Công việc:

1. Kiểm tra ABI, frame-pointer flag và ảnh hưởng inlining/tail-call optimization.
2. Làm stack walker in địa chỉ thô; kiểm tra alignment, biên stack và tiến triển frame trước mỗi lần đọc. Có giới hạn số frame để tránh vòng lặp.
3. Chọn nguồn symbol: ELF kernel và công cụ `nm` hoặc `objdump` của toolchain; chỉ đưa symbol phù hợp vào bảng.
4. Thiết kế build hai bước hoặc cách dành vùng symbol cố định. Việc nhúng bảng có thể dịch địa chỉ sau link: phải kiểm tra bảng nhúng khớp **ELF cuối cùng**, không dùng địa chỉ từ bản link trung gian bị lệch.
5. Cung cấp lookup dùng chung dạng “địa chỉ → tên hàm và offset”, có kết quả rõ ràng khi không tìm thấy.
6. Tích hợp panic, xử lý ngữ cảnh boot/interrupt hoặc không có process bằng cách dừng an toàn khi chưa xác định stack bounds. Không mặc định `myproc()` luôn khác null.
7. Đối chiếu `addr2line` trên chính ELF được chạy. Return address là địa chỉ sau lệnh gọi; quy tắc đối chiếu call site phải xét độ dài lệnh RISC-V, không trừ một hằng số tùy ý.

Giới hạn: frame-pointer backtrace thường chỉ phản ánh stack hiện tại; không mặc nhiên tái dựng toàn bộ chuỗi qua trap hoặc user stack. Inline/tail-call có thể làm mất frame, phải ghi rõ và thiết kế hàm test `noinline` phù hợp.

Hoàn thành khi: panic thử có chuỗi tên hàm đúng; đối chiếu địa chỉ với ELF cuối thành công; frame lỗi không gây dereference ngoài stack; clean build hoạt động trên máy thành viên khác.

### 4.2. Sampling profiler — Tú

Context: timer định kỳ ghi nhận địa chỉ đang thực thi. Tỷ lệ mẫu là ước lượng phân bố thực thi theo phương pháp sampling, không phải số lần gọi hàm hay phần trăm CPU chính xác tuyệt đối.

Công việc:

1. Đọc đường timer/trap của tag thực tế. Lưu PC bị ngắt trước khi các thao tác xử lý trap có thể thay đổi CSR; phân biệt user trap và kernel trap.
2. Chốt phạm vi với nhóm/giảng viên: profile kernel-only hay gồm user. Đề xuất core thu kernel samples, đồng thời đếm/phân loại user samples; hot-loop test dùng kernel workload có timer được phép ngắt để kiểm chứng được symbol kernel.
3. **Không dùng bảng symbol kernel để giải mã PC user.** Nếu cần hiển thị tên hàm user, phải bổ sung metadata ELF đúng chương trình và phân biệt PID/address space; đây là khối lượng riêng cần chốt trước proposal.
4. Dùng cấu trúc có kích thước giới hạn, ưu tiên vùng đệm hoặc counter theo CPU. Ghi nhận số sample bỏ qua/tràn. Không cấp phát, không printf và không lookup symbol tốn thời gian trong timer handler.
5. Thiết kế start/stop/reset/read và quy tắc gọi lặp; chốt reset có yêu cầu dừng trước hay không. Đề xuất ban đầu: stop trước reset để giảm race.
6. Snapshot không trả pointer kernel ra user; kiểm tra con trỏ, kích thước và lỗi `copyout`. Không giữ spinlock qua đường copy có thể gây vấn đề trong phiên bản đang dùng.
7. Làm `kprof`, phân giải symbol ngoài đường lấy mẫu; công bố mẫu số của tỷ lệ, phạm vi kernel/user và giới hạn dữ liệu.
8. Đo overhead, độ ổn định hot-loop detection và khả năng hoạt động nhiều CPU.

Lưu ý đặc biệt: trên xv6, có thể không nhận timer khi interrupts bị tắt hoặc trong một số đường kernel. Không hứa profiler thấy được mọi hot loop. Kết quả có sampling bias; vòng spin của lock có thể không được phản ánh đầy đủ, do đó dùng lockstat để bổ sung.

Hoàn thành khi: start/stop/reset có semantics rõ; test hot loop đạt tiêu chí đã chốt; CPUS=1/4 không lỗi; dữ liệu tràn được báo; `kprof` không gán sai user PC thành kernel symbol; có benchmark overhead.

### 4.3. Lock statistics — Việt

Context: instrumentation đo số lần lấy lock và mức tranh chấp trong chính đường đồng bộ của kernel. Instrumentation không được gọi lại chính nó qua một lock thống kê dùng chung.

Công việc:

1. Kiểm tra `struct spinlock`, `initlock`, `acquire`, `release` của tag thực tế; ưu tiên bao phủ spinlock. Ghi rõ sleep lock có nằm ngoài phạm vi hay không.
2. Chốt ba metric: `acquisitions` = số lần lấy lock thành công; `contended_acquisitions` = số lần lấy lock phải thử thất bại ít nhất một lần; `failed_attempts` = tổng số lần thử thất bại.
3. Không gọi `failed_attempts` là thời gian chờ, clock cycles hoặc nanoseconds. Nếu đo thời gian/cycle thật thì phải thêm phép đo riêng và mô tả overhead.
4. Chọn counter atomic hoặc theo CPU dựa trên thiết kế đã review; counter bình thường có thể race ngay khi nhiều CPU đang tranh chấp lock.
5. Định danh instance bằng ID ổn định; tên chỉ để hiển thị vì nhiều lock có thể trùng tên. Thiết kế registry hữu hạn, overflow rõ ràng, không giữ pointer tới lock đã hết lifetime.
6. Export snapshot và reset theo semantics đã chốt. Đề xuất reset chỉ khi đo được dừng hoặc dùng mốc snapshot nếu khó reset đồng thời; không tùy tiện ghi 0 vào counter đang cập nhật.
7. `lockstat` hiển thị metric có định nghĩa, hỗ trợ sắp xếp theo contention, có tùy chọn raw ổn định cho test.
8. Tạo workload ép contention trên nhiều CPU bằng đồng bộ rõ ràng, timeout và cleanup. Không giả định CPUS=1 sẽ có contention spinlock tương tự CPUS=4.

Hoàn thành khi: bộ đếm khớp các test kiểm soát được; contention cao được phát hiện; không đệ quy/deadlock do instrumentation; thống kê khi `usertests` hoạt động; regression sạch.

### 4.4. Giao diện dùng chung và phạm vi file

| Giao diện | Người triển khai | Người sử dụng | Hợp đồng phải thống nhất trước cuối tuần 2 |
|---|---|---|---|
| Symbol lookup | Sơn | Sơn, Tú | Địa chỉ kernel, tên/offset, unknown symbol, lifetime chuỗi |
| Profiling control/read | Tú | `kprof`, test runner | Start/stop/reset, record version, capacity, overflow, mã lỗi |
| Lockstat read/reset hoặc delta | Việt | `lockstat`, test runner | ID lock, định nghĩa counter, snapshot và semantics reset |
| Host test runner | Việt | Cả nhóm | Lệnh chạy, CPUS, timeout, expected panic, format PASS/FAIL |
| Symbol build pipeline | Sơn | Cả nhóm | Clean build, toolchain, ELF cuối, kiểm tra mismatch |
| Benchmark data | Tú | Cả nhóm | Cấu hình, workload, repetitions, raw CSV, baseline |

Tên syscall cụ thể chưa được quyết định. Có thể dùng syscall điều khiển và syscall đọc cho mỗi module; chốt một ABI nhỏ với record version, không tạo nhiều API trước khi có nhu cầu test. Các lệnh `kprof start/stop/show/reset` và `lockstat` trong kế hoạch là giao diện mong muốn, chưa phải công cụ đã tồn tại.

File dự kiến cần đọc/sửa: `kernel/trap.c`, `kernel/proc.c`, `kernel/proc.h`, `kernel/printf.c`, `kernel/spinlock.c`, `kernel/spinlock.h`, `kernel/defs.h`, các file syscall, `user/user.h`, `user/usys.pl`, `Makefile`, linker script. Có thể thêm `kernel/kprof.c`, `kernel/ksym.c`, `kernel/lockstat.c`, `user/kprof.c`, `user/lockstat.c`, scripts và tests. Đây là đường dẫn trong repository xv6, phải xác minh trước khi sửa.

Phân công file dùng chung: Sơn điều phối Makefile/linker; Tú điều phối trap hooks; Việt điều phối spinlock instrumentation. Mỗi người tự thêm syscall module mình theo quy ước chung và phối hợp số syscall để tránh xung đột.

## 5. Kế hoạch từng tuần

Các tuần dưới đây là **tuần của môn học**, không phải ngày cụ thể tính từ lúc đọc file. Cần điền lịch thực tế từ giảng viên; chưa biết hiện tại là tuần mấy. Nếu bắt đầu muộn, thu gọn phạm vi phụ và tăng tần suất tích hợp, không tự dời deadline môn học.

### Tuần 1 — Môi trường và prototype

Mục tiêu: cả ba build được cùng tag; mỗi phần chứng minh được cơ chế cốt lõi.

| Người | Việc cần làm | Bàn giao/tiêu chí hoàn thành |
|---|---|---|
| Tú | Đọc timer/trap path; xác định nguồn PC và kernel/user; prototype đếm mẫu | Ghi chú trap flow; đếm mẫu với workload kiểm soát; kernel vẫn chạy |
| Sơn | Ghi toolchain/flags; chạy baseline; prototype stack walker | Hướng dẫn build; log baseline; call chain thử in return addresses |
| Việt | Đọc spinlock; prototype acquire counter; phác thảo runner | Counter đơn giản có test; đặc tả PASS/FAIL và timeout |

Cả nhóm: đăng ký đề tài; tạo private repo và tracker; ghi commit gốc; xác minh chính sách AI. Ba prototype có thể chạy độc lập; không đợi backtrace hoàn chỉnh mới nghiên cứu profiler/lockstat.

### Tuần 2 — Chốt thiết kế và giao diện

Mục tiêu: loại bỏ bất định về symbol, PC, counter và snapshot trước proposal.

| Người | Việc cần làm | Bàn giao/tiêu chí hoàn thành |
|---|---|---|
| Tú | Chốt phạm vi kernel/user, cấu trúc sample theo CPU, overflow, ABI profiler | Thiết kế trạng thái; record và mã lỗi; thử thu PC đúng ngữ cảnh |
| Sơn | Sinh symbol table; thiết kế bước link; định nghĩa symbol lookup | Bảng thử khớp ELF cuối; API lookup cho Tú; sơ đồ tổng thể bản đầu |
| Việt | Chốt ID/lifetime lock, metric counters, snapshot; runner tối thiểu | Định nghĩa counter; registry prototype; một test chạy một lệnh |

Cả nhóm: cấp giảng viên quyền đọc từ tuần 2; đưa issue có assignee/estimate lên tracker; chốt đầu vào/đầu ra và expected errors. Hỏi các điểm chưa rõ về rubric và user-space symbolization.

### Tuần 3 — Proposal và core ban đầu

Mục tiêu: nộp proposal 2–3 trang dựa trên prototype thật.

| Người | Việc cần làm | Bàn giao/tiêu chí hoàn thành |
|---|---|---|
| Tú | Start/stop; thu mẫu hữu hạn; test trạng thái và error paths | Start/stop hoạt động; báo overflow; viết phần profiler của proposal |
| Sơn | Lookup tên/offset; backtrace gọi từ panic; kiểm tra biên stack | Panic thử có tên hàm; viết phần backtrace/build của proposal |
| Việt | Acquisition/contention counters; export snapshot sơ bộ; CI smoke | Counter có oracle kiểm chứng; CI build/boot chạy; phần lockstat proposal |

Cả nhóm: proposal gồm cấu trúc dữ liệu, file sửa, API, test plan, phân công và rủi ro. Không xem pseudocode là tính năng đã hoàn thành.

### Tuần 4 — Công cụ người dùng và tích hợp sớm

Mục tiêu: ba đường dữ liệu có bản dùng được và kiểm thử được.

| Người | Việc cần làm | Bàn giao/tiêu chí hoàn thành |
|---|---|---|
| Tú | `kprof` đọc/hiển thị snapshot, nối symbol lookup, reset | Kết quả kernel/user phân biệt; invalid pointer/size trả lỗi |
| Sơn | Test `addr2line`; unknown symbol; frame lỗi; kiểm tra clean build | Backtrace test tự động; symbol verifier chạy trên ELF cuối |
| Việt | `lockstat`; sắp xếp; reset/delta; timeout và expected-panic runner | Tool hiển thị đúng metric; runner xử lý reboot sau panic |

Cả nhóm: tích hợp trên nhánh chung, sửa xung đột ngay, regression sau thay đổi chung. Có ít nhất 10 case có oracle, không chỉ log đẹp.

### Tuần 5 — Checkpoint

Mục tiêu nội bộ: cả ba module chạy end-to-end; ít nhất 15 test tự động và demo 10 phút. Đây là mục tiêu của nhóm cao hơn yêu cầu “core partly working” của checkpoint.

| Người | Việc cần làm | Bàn giao/tiêu chí hoàn thành |
|---|---|---|
| Tú | Hot-loop detection trên workload đã chốt; multi-CPU smoke; benchmark sơ bộ | `kprof` xác định vùng nóng với ngưỡng có căn cứ |
| Sơn | Demo panic symbolized, đối chiếu ELF; chạy regression | Log backtrace đúng; `usertests/grind` không có regression mới chưa xử lý |
| Việt | Demo lockstat với contention; CI chạy suite ban đầu | Counter phản ánh workload; CI có artifacts PASS/FAIL |

Cả nhóm: ghi phản hồi giảng viên và cập nhật backlog. Nếu một core chưa chạy, loại bỏ toàn bộ stretch và ưu tiên lỗi correctness.

### Tuần 6 — Kiểm thử đầy đủ và stress

Mục tiêu: đạt ít nhất 25 case độc lập có ý nghĩa, xử lý race/lifetime/error paths.

| Người | Việc cần làm | Bàn giao/tiêu chí hoàn thành |
|---|---|---|
| Tú | Tests snapshot/start-stop/reset, overflow, bad user buffer, profiling CPUS=1/4 | Test profiler tích hợp runner; race được sửa hoặc ghi blocker cụ thể |
| Sơn | Frame corruption tests, symbol mismatch, leak hooks cần thiết, regression | Test backtrace và resource checks; build tái lập trên máy khác |
| Việt | Counter concurrency, registry capacity, stress automation, CI artifacts | Suite một lệnh; stress bắt đầu chạy đủ 10 lần mỗi cấu hình |

Cả nhóm: leak checks sau cleanup; mỗi người sửa module mình. Việt duy trì runner và CI, Sơn cung cấp test/build/regression tương thích runner; không đổi người sở hữu giữa tuần.

### Tuần 7 — Đo lường và hoàn thiện

Mục tiêu: có số liệu tin cậy, stress hoàn tất, không còn thiếu core.

| Người | Việc cần làm | Bàn giao/tiêu chí hoàn thành |
|---|---|---|
| Tú | Chạy benchmark các cấu hình; xử lý dữ liệu, biểu đồ và sampling bias | ≥5 lần/điểm; raw data, mean/spread; kết luận có giới hạn |
| Sơn | Regression cuối đợt; kiểm tra symbol artifact; tài liệu kiến trúc và hạn chế | Log baseline/final; sơ đồ phản ánh code thật; phần báo cáo backtrace |
| Việt | Long stress, lockstat overhead data, demo script | CPUS=1/4 mỗi stress ≥10 lần; kịch bản chạy/reboot có timeout |

Cả nhóm: viết đầy đủ các phần báo cáo; review chéo mã quan trọng. Chỉ cân nhắc stretch khi không gây rủi ro lịch; mặc định tuần này không thêm subsystem mới.

### Tuần 8 — Code freeze

Mục tiêu: final tag build và test được từ clean checkout, mọi tài liệu khớp đúng commit.

| Người | Việc cần làm | Bàn giao/tiêu chí hoàn thành |
|---|---|---|
| Tú | Xác minh profiler/benchmark; kiểm tra report số liệu | Code, cấu hình và kết quả nhất quán; phần profiler hoàn chỉnh |
| Sơn | Clean build từ checkout khác; symbol check; regression final | Final ELF/symbol khớp; hướng dẫn build tái lập |
| Việt | Chạy suite/stress final; đóng gói log và demo script | Một lệnh test hoạt động; không mất log; hướng dẫn demo đầy đủ |

Cả nhóm: kiểm tra contribution statements; tạo tag `final` sau khi checks đạt; lưu commit, toolchain và checksum artifacts. Bản report đầy đủ sẵn sàng, xác nhận hạn nộp. Không âm thầm sửa hoặc di chuyển tag sau freeze; xử lý theo quy định giảng viên.

### Tuần 9 — Demo và báo cáo

Mục tiêu: trình bày trong 20 phút gồm Q&A, không phụ thuộc vào thao tác chưa thử.

- Sơn trình bày backtrace, symbol build và đối chiếu `addr2line`.
- Tú trình bày profiler, workload, kết quả và overhead.
- Việt trình bày lockstat, contention, bộ test và stress.
- Cả ba nộp báo cáo 15–20 trang và đóng góp cá nhân; trả lời invariant, trade-off, lỗi từng gặp và cách kiểm chứng.

## 6. Danh sách test dự kiến

Mục tiêu 32 case dưới đây là kế hoạch, chưa phải test đã tồn tại hoặc đã pass. Mỗi case cần script/code kiểm tra expected result và in PASS/FAIL. Chạy cùng case nhiều lần hoặc đổi CPUS không tự biến nó thành nhiều case khác nhau để đủ số lượng.

| ID | Người chính | Test và kết quả cần kiểm chứng |
|---|---|---|
| BT01 | Sơn | Call chain `noinline` có ít nhất ba frame theo thứ tự mong đợi |
| BT02 | Sơn | Symbol lookup trả đúng tên tại địa chỉ đầu hàm |
| BT03 | Sơn | Địa chỉ bên trong hàm trả đúng offset |
| BT04 | Sơn | Địa chỉ ngoài vùng symbol trả unknown an toàn |
| BT05 | Sơn | Backtrace đối chiếu được với `addr2line` trên ELF cuối |
| BT06 | Sơn | Frame pointer sai alignment bị từ chối trước dereference |
| BT07 | Sơn | Frame ngoài stack bounds bị từ chối an toàn |
| BT08 | Sơn | Frame lặp/không tiến triển dừng hữu hạn |
| BT09 | Sơn | Panic có kiểm soát sinh backtrace, runner nhận đúng expected panic |
| BT10 | Sơn | Clean rebuild: verifier phát hiện symbol table khớp ELF cuối |
| KP01 | Tú | Khi tắt profiler, số mẫu không tăng sau workload |
| KP02 | Tú | Start rồi workload tạo mẫu ở phạm vi được khai báo |
| KP03 | Tú | Stop: số liệu ổn định sau khi hoàn tất đồng bộ dừng |
| KP04 | Tú | Reset tuân thủ state contract, xóa dữ liệu đúng phạm vi |
| KP05 | Tú | Gọi start/stop lặp trả kết quả đúng contract, không leak |
| KP06 | Tú | Hot loop được nhận diện theo ngưỡng thống kê đã chốt, đủ thời gian đo |
| KP07 | Tú | User PC không bị giải mã sai bằng symbol kernel |
| KP08 | Tú | Đọc snapshot khi đang lấy mẫu không tạo record hỏng |
| KP09 | Tú | Buffer đầy: không ghi vượt; dropped/overflow được báo |
| KP10 | Tú | User buffer lỗi, capacity sai hoặc quá nhỏ trả kết quả đúng contract |
| LS01 | Việt | N lượt acquire kiểm soát được cho delta N trên lock test |
| LS02 | Việt | Uncontended workload không báo contended acquisition trên lock test |
| LS03 | Việt | Contention chủ động trên CPUS=4 có failed attempts và contention >0 |
| LS04 | Việt | contended_acquisitions ≤ acquisitions với snapshot cùng kỳ đo |
| LS05 | Việt | Hai lock cùng tên vẫn có ID phân biệt |
| LS06 | Việt | Snapshot khi nhiều CPU cập nhật không có record hỏng |
| LS07 | Việt | Reset hoặc baseline-delta tuân thủ semantics đã chốt |
| LS08 | Việt | Registry/capacity chạm giới hạn được xử lý không crash |
| IN01 | Việt | Chạy profiler và lockstat cùng workload, không deadlock; timeout rõ |
| IN02 | Sơn | Sau suite và cleanup, free-page/free-inode không thất thoát ngoài allowance đã giải thích |
| IN03 | Sơn | Stock `usertests` pass theo phạm vi chính thức |
| IN04 | Sơn | `grind` chạy thời lượng đã công bố, không panic/hang ngoài dự kiến |

Các test white-box như frame hỏng hoặc capacity nhỏ cần hook/build test riêng; không sửa bừa stack kernel đang chạy. Nếu tạo test-only syscall, bảo vệ bằng cấu hình và ghi rõ khác biệt với final build. Panic tests chạy QEMU riêng hoặc reboot rõ ràng; không để panic làm mất toàn bộ suite.

Stress bổ sung: workload process creation/exit, profiler bật/tắt/đọc theo contract, đọc lockstat và I/O song song. Việt tự động hóa CPUS=1/4, mỗi cấu hình ít nhất 10 lần; mỗi người định nghĩa oracle phần mình. Không dùng contention >0 trên CPUS=1 làm điều kiện bắt buộc cho spinlock test.

## 7. Kế hoạch benchmark

Tú tổng hợp quy trình; Sơn cung cấp stock build và clean build; Việt cung cấp workload contention, runner và raw log.

| Cấu hình | Mục đích |
|---|---|
| Stock xv6 đúng tag | Baseline không có toolkit |
| Toolkit build, profiler tắt | Đo chi phí còn lại, gồm lock instrumentation nếu luôn bật |
| Toolkit, profiler bật | Đo chi phí tăng thêm do sampling |
| Lock instrumentation tắt/bật qua tùy chọn build, nếu khả thi | Tách riêng chi phí lockstat; ghi rõ những phần khác phải giống nhau |

Workload: kernel CPU loop có thể bị timer ngắt; user CPU-heavy để kiểm tra phân loại; syscall-heavy; file I/O; workload contention. Mỗi cấu hình/workload/CPUS là một điểm dữ liệu, chạy ít nhất 5 lần, lưu thời gian từng lần, mean và độ lệch chuẩn hoặc min–max. Giữ QEMU, RAM, CPU, image và compiler flags tương đương; ghi commit/config, thời gian đo, máy host và yếu tố nhiễu.

Chỉ số: elapsed time, overhead tương đối so với baseline, sample count, sample distribution, dropped samples, acquisition/contended acquisition/failed attempts. CPU percentage từ sample cần nêu mẫu số và không gọi là thời gian CPU chính xác. Không đặt cam kết overhead dưới một phần trăm cụ thể trước khi có dữ liệu.

Backtrace: ưu tiên correctness và chi phí bộ nhớ symbol table; không gọi panic latency là benchmark hiệu năng chính. Báo cáo những vùng profiler không quan sát được do interrupts tắt.

## 8. Quy trình phối hợp và Definition of Done

Mỗi task trên tracker có: ID, context, mục tiêu, owner, dependency, file dự kiến, acceptance criteria, test, estimate và link PR. Nhánh gợi ý: `feature/backtrace`, `feature/kprof`, `feature/lockstat`; chia PR nhỏ theo chức năng, không chờ tuần 5 mới merge toàn bộ.

Một task được coi là Done khi:

1. Code chạy trên đúng base/tag và build không có lỗi mới.
2. Có test kiểm tra kết quả; ghi lệnh, cấu hình và log thật.
3. Có review chéo; invariants và lỗi đầu vào được giải thích.
4. Tài liệu API/sơ đồ cập nhật nếu thay đổi contract.
5. Không còn lỗi đã biết bị che bằng timeout quá rộng, bỏ assert hoặc bỏ regression.
6. Chủ task giải thích được code và trade-off.

Template báo cáo tuần: việc hứa tuần trước → kết quả thực tế; từng người đã làm gì và commit nào; test pass/fail; số liệu mới; blocker/rủi ro; task tuần sau và Definition of Done. Không lấy số dòng code làm thước đo duy nhất của đóng góp.

## 9. Cấu trúc bàn giao đề xuất trong repository

```text
README.md                  # Build, chạy toolkit, test và demo
docs/design.md             # Kiến trúc, invariants, phạm vi kernel/user
docs/api.md                # ABI, state, errors, snapshot/reset
docs/weekly/               # Báo cáo tuần
docs/decisions.md           # Quyết định và lý do, ngày/commit
docs/handoff.md             # Trạng thái thực tế để người hoặc AI tiếp tục
tests/                     # Test và workload
scripts/                   # Symbol generation, runner, benchmark, demo
results/                   # Raw logs, metadata, số liệu/biểu đồ phù hợp
```

Đây là cấu trúc đề xuất, không phải thư mục đã được tạo. Theo cấu trúc repo thực tế nếu đã có quy ước. Không commit secret/token hoặc artifact quá lớn; giữ đủ nguồn và script để tái tạo kết quả.

## 10. Hướng dẫn dùng tài liệu này làm context cho AI

Gửi file này cùng repository thực tế hoặc các file liên quan. AI chỉ có file kế hoạch chưa thể biết code đã triển khai đến đâu. Thành viên điền trạng thái và mục tiêu cụ thể trước mỗi phiên; giữ các ràng buộc môn học và chính sách AI.

Prompt dùng chung:

```text
Tôi đang làm đề tài 3 Kernel Debugging and Observability Toolkit trên xv6-riscv.
Hãy đọc file kế hoạch đính kèm và repository thực tế trước khi sửa code.
Tôi là: [Tú / Sơn / Việt].
Phần tôi phụ trách: [profiler / backtrace và symbol / lock statistics].
Base tag/commit: [...]. Nhánh hiện tại: [...].
Đang ở tuần môn học: [...]. Task cần thực hiện trong phiên này: [...].
Đã hoàn thành: [...]. Lỗi hiện tại và log: [...].
Lệnh build/test và môi trường: [...].

Trước hết xác minh trạng thái code, compiler flags, trap path và các API liên quan.
Phân biệt yêu cầu catalogue với thiết kế đề xuất trong kế hoạch; không tự mở rộng scope.
Làm task thành thay đổi nhỏ có thể review; giữ thay đổi đang có của nhóm.
Nếu thiếu quyết định ảnh hưởng module khác, nêu rõ và chốt contract trước khi sửa phần phụ thuộc.
Viết/chạy test có oracle phù hợp. Không báo pass nếu chưa chạy; ghi chính xác phần chưa kiểm chứng.
Cuối phiên giải thích thay đổi, invariants, file sửa, lệnh/kết quả test, hạn chế và bước tiếp theo.
Giải thích đủ để tôi tự hiểu và bảo vệ code trong Q&A.
```

Task khởi đầu phù hợp cho từng người:

| Người | Context bổ sung cho AI | Mục tiêu phiên đầu |
|---|---|---|
| Tú | Phụ trách timer sampling; kernel/user PC phải phân biệt; không symbol hóa user bằng kernel table | Đọc trap/timer của tag, chỉ ra điểm lấy PC đúng, làm prototype đếm mẫu tối thiểu và kiểm tra ổn định |
| Sơn | Phụ trách stack walker và build symbol; địa chỉ phải khớp final ELF | Kiểm tra frame-pointer flags, tạo call chain kiểm soát, in return addresses với bounds checks |
| Việt | Phụ trách counters trong spinlock; instrumentation không được đệ quy | Đọc acquire/release, định nghĩa metric và prototype acquisition counter có test kiểm soát |

Template cập nhật `docs/handoff.md` sau mỗi phiên:

```text
Ngày / thành viên / nhánh / commit:
Task và mục tiêu:
Đã hoàn thành, file đã sửa:
API/invariant đã quyết định hoặc thay đổi:
Lệnh test và kết quả thực tế:
Chưa kiểm chứng hoặc lỗi còn lại:
Dependency với hai thành viên khác:
Task nhỏ tiếp theo và tiêu chí hoàn thành:
```

## 11. Danh sách việc khởi động ngay

1. Cả nhóm lấy đúng tag, ngày deadline và xác nhận tuần môn học hiện tại; thống nhất môi trường build.
2. Sơn ghi baseline build/boot, `usertests`, `grind` và toolchain.
3. Tú đọc timer/trap và chốt câu hỏi về phạm vi kernel/user profiling.
4. Việt thiết lập tracker task của lockstat và runner tối thiểu; phối hợp cả nhóm để tạo quyền truy cập cần thiết.
5. Mỗi người làm prototype tuần 1 của mình, lưu log và giải thích được cơ chế.
6. Cả nhóm review ba prototype, chốt API trước cuối tuần 2 rồi hoàn thiện proposal.

Nguyên tắc ưu tiên xuyên suốt: correctness và khả năng giải thích → tích hợp → kiểm thử → đo lường → trình bày. Mọi kết quả trong báo cáo cuối phải truy ngược được đến code, cấu hình và log thực tế.
