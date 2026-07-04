# Chương 10 — Tư duy thiết kế

Đây là chương giúp bạn **suy nghĩ như người thiết kế hệ thống**, không chỉ chép code. Mỗi
mục là một câu hỏi "tại sao" thường gặp khi bảo vệ đồ án.

## 10.1. Tại sao tách Controller ↔ Service (không nhồi hết vào Controller)?

- **Controller** chỉ nên lo việc **giao tiếp HTTP**: đọc request, đọc claim, phân quyền, gọi
  Service, trả mã trạng thái. Nó là "lễ tân".
- **Service** lo **nghiệp vụ**: transaction, luật, tính toán, đẩy realtime. Nó là "chuyên gia".
- **Nếu nhồi hết vào Controller:** logic phức tạp (như `CreateOrderAsync` ~130 dòng) sẽ nằm
  lẫn với chuyện HTTP; **khó test**, **khó tái dùng** (VD `PaymentController` cũng cần gọi
  `AdvanceToProcessingAfterPaymentAsync`/`ClearPurchasedItemsAsync` của `OrderService`), và
  vi phạm nguyên tắc "một việc một chỗ" (Single Responsibility).

Bằng chứng tái dùng trong dự án: `OrderService` được **cả** `OrderController` và
`PaymentController` dùng chung.

## 10.2. Tại sao Entity khác DTO?

- **Entity** (VD `NguoiDung`) ánh xạ **bảng DB**, chứa cả thứ **không được lộ** ra ngoài
  (`MatKhauHash`).
- **DTO** (VD `LoginDTO`, `CreateOrderDTO`) định nghĩa **đúng dữ liệu API cần** nhận/trả.
- **Nếu dùng Entity trực tiếp ở API:** rủi ro **lộ `MatKhauHash`**, hoặc client gửi lên cả
  những trường không được phép sửa (over-posting, VD tự đặt `MaVaiTro = 4` để thành admin).
  DTO là "bộ lọc" bảo vệ ranh giới.
- SyncChain thấy rõ điều này: `AuthService.Login` trả về **object ẩn danh** chỉ gồm
  `MaNguoiDung/TenDangNhap/Email/role` — **không** kèm hash.

## 10.3. Tại sao dùng Interface cho một số Service?

- `INotificationService`, `IAuditService`, `ISystemErrorLogService` có **interface**; nhiều
  service khác thì không.
- **Lý do có interface:** những service này được **nhiều nơi phụ thuộc** và là **ứng viên để
  thay/test** (VD test có thể tiêm `INotificationService` giả để không đẩy SignalR thật). Nó
  cũng tránh phụ thuộc vòng và giảm khớp nối.
- **Lý do các service khác không cần interface:** ở quy mô đồ án, tạo interface cho *mọi* thứ
  là **thừa** (over-engineering). Nguyên tắc: **thêm trừu tượng khi có lợi ích thật**, không
  phải theo phản xạ.

## 10.4. Tại sao dùng Dependency Injection?

- Để **không hardcode "ai tạo ai"**. `Program.cs` khai một lần cách tạo mọi thứ; các class
  chỉ **khai báo nhu cầu** ở constructor.
- DI quản lý **vòng đời** đúng:
  - **Scoped** (VD `AppDbContext`, `OrderService`): **một bản mỗi HTTP request**. Quan trọng
    vì `DbContext` **không** an toàn dùng chung nhiều request/luồng.
  - **Singleton** (VD `VnPayService`, `SystemErrorLogService`): **một bản cả app** — hợp cho
    thứ không giữ trạng thái per-request.
  - **HttpClient-typed** (`MoMoService` qua `AddHttpClient`): để dùng `HttpClient` đúng cách
    (không cạn cổng).
  - **HostedService** (`ShippingAutoCompletionService`): chạy nền suốt vòng đời app.
- **Nếu tự `new`:** dễ tạo `DbContext` sai vòng đời (dùng chung → lỗi đồng thời), khó thay thế
  khi test.

## 10.5. Tại sao cần transaction?

Một thao tác nghiệp vụ thường gồm **nhiều bước ghi**. VD tạo đơn: ghi `DonHang` → trừ kho
nhiều sản phẩm → ghi `ChiTietDonHang` → ghi audit. Nếu bước 3 lỗi mà bước 1–2 đã ghi, ta có
**đơn không có chi tiết** và **kho đã trừ oan**. Transaction đảm bảo **hoặc xong tất cả, hoặc
hoàn tác sạch** — dữ liệu không bao giờ dở dang.

## 10.6. Tại sao dùng ConcurrencyVersion + trừ kho có điều kiện?

Đây là **điểm cốt lõi của môn mạng**: nhiều client thao tác đồng thời.

- **Không kiểm soát:** hai người đọc đơn (version cũ), cùng ghi đè → "lost update"; hoặc hai
  khách mua món cuối → tồn về **âm** (oversell).
- **Cách SyncChain:** đặt điều kiện *ngay trong câu UPDATE*:
  - Đổi trạng thái: `WHERE ConcurrencyVersion = expected` + `version + 1`. Chỉ một người khớp
    → thắng; người kia `changedRows = 0` → 409.
  - Trừ kho: `WHERE SoLuongTon >= quantity`. DB **tự tuần tự hóa** các UPDATE trên cùng hàng
    → không thể trừ âm.
- Đây là kiểu **optimistic concurrency** (lạc quan): không khóa trước, chỉ phát hiện xung đột
  khi ghi — hiệu năng tốt cho web.

## 10.7. Tại sao dùng Idempotency Key?

Mạng không tin cậy: client gửi "tạo đơn", mất phản hồi, **gửi lại** → nguy cơ **2 đơn**.
`IdempotencyKey` (unique index) + logic **replay** đảm bảo lần gửi lại trả về **đúng đơn cũ**,
không tạo mới. Đây là chuẩn mực cho API "tạo" qua mạng.

## 10.8. Tại sao dùng Async/Await?

Server phải phục vụ **nhiều client cùng lúc**. Thao tác I/O (DB, mạng) tốn thời gian *chờ*.
`async/await` cho phép luồng **không đứng chờ** mà đi phục vụ request khác trong lúc chờ DB
→ **thông lượng cao hơn** với cùng số luồng. Đó là lý do các method nghiệp vụ đều
`...Async` và dùng `await _db.SaveChangesAsync()`.

## 10.9. Tại sao trạng thái đơn tập trung ở `OrderStatuses.cs`?

Nếu mỗi nơi tự viết chuỗi `"pending"`, `"Pending"`, `"Draft"`... sẽ sinh **bug so sánh
chuỗi** (đã từng xảy ra với bộ PascalCase cũ). **Một nguồn chân lý** (`OrderStatuses` +
`CanTransition`) khiến luật chuyển trạng thái **có một chỗ để đọc và một chỗ để sửa**. Client
cũng map qua `OrderStatusDisplay` để đồng bộ.

## 10.10. Tại sao có `/health` và xử lý lỗi tập trung?

- **`/health`:** để client **biết trước** server sống/DB nối được, báo lỗi thân thiện thay vì
  crash giữa chừng (`LoginPage.OnAppearing`). Cũng là "đèn tín hiệu" cho script `run-all`
  chờ backend sẵn sàng.
- **Xử lý lỗi tập trung** (`ApiExceptionHandler`, `UseStatusCodePages`): mọi lỗi trả về
  **cùng một hình dạng JSON** (`code/message/details/traceId`) → client xử lý nhất quán,
  người debug lần theo `traceId`.

## 10.11. Tại sao đọc DB config từ `.env` chứ không phải `appsettings.json`?

Để **không commit bí mật** (chuỗi kết nối có mật khẩu) và **dùng chung** giữa backend .NET và
prototype Node. `.env` bị gitignore; mỗi người tự tạo từ `.env.example`. `appsettings.json`
chỉ giữ cấu hình **không nhạy cảm / sandbox** (JWT demo, VNPay/MoMo test).

## 10.12. Tại sao `EnsureCreated()` mà vẫn có `Migrations/`? (một điểm cần biết)

- **`EnsureCreated()`** dựng nhanh schema từ model — **tiện cho đồ án**, chạy phát là có DB.
- Nhưng nó **không tương thích tốt** với migration (không áp dụng lịch sử migration). Vì thế
  dự án **bù** bằng loạt `ALTER TABLE ... IF NOT EXISTS` trong `Program.cs` để thêm cột/bảng
  mới trên DB đã tồn tại.
- **Bài học thiết kế:** với sản phẩm thật, nên chọn **hẳn một hướng** — dùng migration
  (`db.Database.Migrate()`) làm nguồn chân lý schema. Cách hiện tại là **đánh đổi có chủ đích**
  ưu tiên sự đơn giản khi trình diễn. (Đây là điều tốt để nêu khi bảo vệ: hiểu rõ đánh đổi.)

## 10.13. Tại sao không có Repository riêng?

Vì **`DbSet<T>` của EF Core đã là repository + Unit of Work**. Thêm một lớp `Repository`
bọc quanh EF ở quy mô này chủ yếu tạo **code lặp** mà ít lợi ích. Service gọi thẳng `_db` là
lựa chọn thực dụng. (Nếu sau này cần đổi ORM hoặc mock DB nặng, mới cân nhắc thêm repository.)

## 10.14. Bảng "quyết định thiết kế → đánh đổi"

| Quyết định | Được | Mất |
|-----------|------|-----|
| JWT stateless | Mở rộng dễ, không giữ session | Không thu hồi token trước hạn (2h) |
| Optimistic concurrency | Hiệu năng cao, không khóa | Client phải xử lý 409 (tải lại) |
| `EnsureCreated` + ALTER | Chạy ngay, ít rào cản | Không "chuẩn" như migration thuần |
| Ít thư viện ngoài | Dễ hiểu, kiểm soát rõ | Viết tay nhiều hơn |
| Map DTO thủ công | Rõ ràng, dễ debug | Nhiều `Select(new {...})` lặp |

---

➡️ Tiếp theo: [11_Build_Run.md](11_Build_Run.md) — clone, build, chạy, test, gỡ lỗi.
