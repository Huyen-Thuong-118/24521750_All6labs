# Chương 6 — Phân tích từng file (đi sâu)

Chương này mổ xẻ các file **cốt lõi** theo khuôn: *vai trò → vì sao cần → class/method →
gọi ai / ai gọi → phụ thuộc (DI) → có đụng DB không → luồng dữ liệu → trích code → giải
thích*. Các file phụ được liệt kê gọn ở cuối để không bỏ sót.

> Ký hiệu: "→" nghĩa là "gọi tới". Đường dẫn tính từ gốc repo.

---

## PHẦN A — BACKEND (`SyncChain.API`)

### A1. `SyncChain.API/Program.cs`  ⭐ file quan trọng nhất backend

- **Vai trò:** điểm khởi động (entry point). Cấu hình toàn bộ: nạp `.env`, DI, JWT, Swagger,
  policy phân quyền, tự dựng schema + seed, middleware pipeline, map SignalR hub, `/health`.
- **Vì sao cần:** không có nó, ứng dụng web **không tồn tại** — đây là nơi ASP.NET Core được
  cấu hình và khởi chạy (`app.Run()`).
- **Không có class:** dùng *top-level statements* (C# hiện đại), chạy tuần tự từ trên xuống.

**Các khối chính (theo thứ tự thực thi):**

1. **Nạp `.env`:** `EnvFileLoader.Load();` (dòng 18) — đưa `DATABASE_URL` vào biến môi trường.
2. **Đăng ký Controllers + tùy biến lỗi validation** (dòng 22–51): khi model không hợp lệ,
   trả `ApiErrorResponse { code = "VALIDATION_ERROR", details, traceId }` **và** ghi
   `SystemErrorLog`. → Lỗi 400 luôn có định dạng thống nhất.
3. **Đăng ký hạ tầng lỗi + SignalR + audit** (dòng 52–59): `AddExceptionHandler`,
   `AddProblemDetails`, `AddSignalR`, `AddHttpContextAccessor`, `IAuditContextAccessor`,
   `IAuditService`, `ISystemErrorLogService`, `AuthService`.
4. **Cấu hình DB** (dòng 61–74): đọc `DATABASE_URL`; nếu trống → **ném lỗi rõ ràng**; chuyển
   thành chuỗi Npgsql; `AddDbContext<AppDbContext>(UseNpgsql(...))`.
   ```csharp
   var databaseUrl = builder.Configuration["DATABASE_URL"];
   if (string.IsNullOrWhiteSpace(databaseUrl))
       throw new InvalidOperationException("Chua cau hinh DATABASE_URL...");
   var connectionString = EnvFileLoader.ToPostgreSqlConnectionString(databaseUrl);
   builder.Services.AddDbContext<AppDbContext>(o => o.UseNpgsql(connectionString));
   ```
5. **Swagger** (dòng 76–109): tài liệu API tại `/swagger`, có ô nhập `Bearer {token}`.
6. **JWT Bearer** (dòng 111–143): tham số kiểm token (issuer/audience/lifetime/signing key
   từ `Jwt:*`). Đặc biệt `OnMessageReceived` (dòng 133–142): nếu path bắt đầu `/hubs` thì lấy
   token từ query `access_token` → để **SignalR** xác thực được qua WebSocket.
7. **Policy phân quyền** (dòng 145–191): định nghĩa các policy theo role — `AdminOnly`,
   `InternalOnly`, `StaffOrAbove`, `ManagerOrAdmin`, `ProductRead/Write`,
   `InventoryRead/Write/Approve`, `OrderWrite/Manage`, `AuditRead`, `SystemErrorLogRead`,
   `ReportView`, `RevenueView`. Controller gắn bằng `[Authorize(Policy="...")]`.
8. **Đăng ký Service nghiệp vụ** (dòng 193–207): `ProductService`, `OrderService`,
   `WarehouseReceiptService`, `WarehouseIssueService`, `InventoryService`, `ShippingService`,
   `DeliveryEstimateService`, `ChatService`, `AddressService`, `INotificationService`,
   `CartService`, `EmailService`, `VnPayService` (**Singleton**),
   `MoMoService` (**HttpClient-typed**), và **hosted service**
   `ShippingAutoCompletionService` (chạy nền tự hoàn tất giao hàng).
9. **Tự dựng schema + seed** (dòng 211–430): trong một scope:
   - `db.Database.EnsureCreated()` — tạo mọi bảng từ model EF.
   - Kiểm bảng `DonHang`, nếu thiếu thì `CreateTables()`.
   - Loạt `ALTER TABLE ... ADD COLUMN IF NOT EXISTS` + `CREATE TABLE IF NOT EXISTS` cho các
     cột/bảng thêm sau (địa chỉ giao, chat, giỏ hàng, thanh toán...). → *nâng cấp DB "tại
     chỗ" không cần migration*.
   - Seed 4 role (`customer/staff/manager/admin`) và tài khoản `admin@gmail.com` mật khẩu
     `123456` (băm BCrypt).
10. **Middleware pipeline** (dòng 432–503, thứ tự rất quan trọng):
    `[HTTP log]` → `UseExceptionHandler` → `UseStatusCodePages` (401/403 → JSON + log) →
    `UseSwagger/UI` → `UseStaticFiles` → `UseAuthentication` → `UseAuthorization` →
    `MapControllers`.
11. **Map hub** (dòng 504–505): `/hubs/chat` (ChatHub), `/hubs/order` (OrderHub).
12. **`/health`** (dòng 509–531): endpoint ẩn danh, `CanConnectAsync()` → 200/503, không bao
    giờ ném exception.
13. `app.Run();` — bắt đầu lắng nghe.

- **Đụng DB:** có (seed + health).
- **Ai gọi Program:** .NET runtime khi chạy `dotnet run --project SyncChain.API`.

---

### A2. `SyncChain.API/Configuration/EnvFileLoader.cs`

- **Vai trò:** đọc `.env` và biến `DATABASE_URL` (dạng URL) thành **chuỗi kết nối Npgsql**.
- **Vì sao cần:** cả backend .NET và prototype Node dùng **chung một `.env`**. .NET không tự
  đọc `.env`, nên cần loader này. Tách DB config khỏi `appsettings.json` để **không commit
  bí mật**.
- **Method:**
  - `Load()` — duyệt `.env` (tìm ngược lên từ thư mục hiện tại và `BaseDirectory`), bỏ dòng
    trống/comment, `SetEnvironmentVariable` **nếu chưa có** (biến môi trường thật được ưu tiên).
  - `FindEnvFile()` — đi ngược cây thư mục tìm `.env` (nên chạy từ thư mục con vẫn thấy).
  - `ToPostgreSqlConnectionString(url)` — parse `postgresql://user:pass@host:port/db` bằng
    `Uri`, tách user/pass/host/db, dựng `NpgsqlConnectionStringBuilder`. Ném lỗi rõ ràng nếu
    URL sai định dạng.
  - `Unquote()` — bỏ dấu nháy quanh giá trị.
- **Ai gọi:** `Program.cs` (`Load()` ở đầu; `ToPostgreSqlConnectionString` khi cấu hình DB).
- **Đụng DB:** không (chỉ dựng chuỗi kết nối).

---

### A3. `SyncChain.API/Data/AppDbContext.cs`

- **Vai trò:** "phiên dịch viên trưởng" EF Core. Khai báo mọi bảng (`DbSet<>`) và cấu hình
  quan hệ/ràng buộc trong `OnModelCreating`.
- **Vì sao cần:** EF Core cần biết có entity nào, ánh xạ ra bảng nào, khóa ngoại/chỉ mục ra
  sao. Không có nó → không truy vấn DB kiểu C# được.
- **Điểm đáng chú ý trong `OnModelCreating`:**
  - **Idempotency:** `DonHang.IdempotencyKey` là **unique index** → chống tạo trùng đơn.
  - **Concurrency token:** `DonHang.ConcurrencyVersion` và `VanChuyen.ConcurrencyVersion` là
    `IsConcurrencyToken()` → nền tảng cho kiểm soát đồng thời.
  - **AuditLog:** nhiều chỉ mục theo (user, thời gian), (đối tượng, thời gian)... và các cột
    `DuLieuTruoc/DuLieuSau/Metadata` kiểu **`jsonb`** (PostgreSQL).
  - **Chat:** map tên bảng số ít (`ChatConversation`, `ChatMessage`...), unique index cuộc
    trò chuyện 1-1 có **filter** `"LaNhom" = FALSE`, cascade khi xóa hội thoại.
  - **Vận chuyển:** `VanChuyen` 1-1 với `DonHang`, `MaVanDon` unique có filter, check
    constraint `PhiVanChuyen >= 0`, precision `(18,2)`.
  - **Kho:** phiếu nhập/xuất có `SoPhieu` unique, chi tiết unique theo (phiếu, sản phẩm),
    cascade chi tiết; `GiaoDichKho` khóa ngoại mềm (`SetNull`) tới đơn/phiếu.
  - **Giỏ hàng:** cấu hình FK tường minh để tránh EF tạo shadow FK.
- **Ai gọi:** mọi Service/Controller cần DB (được DI tiêm vào), và `Program.cs` (seed).

---

### A4. `SyncChain.API/Controllers/AuthController.cs`

- **Vai trò:** cửa vào cho xác thực: đăng ký, đăng nhập, xem/sửa hồ sơ, đổi mật khẩu.
- **Vì sao cần:** nếu không có, người dùng **không thể lấy JWT** → không dùng được API nào
  cần đăng nhập. Nó là "quầy vé".
- **Phụ thuộc (DI):** `AuthService _auth` (tiêm qua constructor).
- **Endpoint:**
  | Method | Route | Quyền | Gọi |
  |--------|-------|-------|-----|
  | POST | `api/Auth/register` | public | `_auth.Register(dto)` |
  | POST | `api/Auth/login` | public | `_auth.Login(dto)` |
  | GET | `api/Auth/profile` | `[Authorize]` | `_auth.GetProfile(userId)` |
  | PUT | `api/Auth/profile` | `[Authorize]` | `_auth.UpdateProfile(userId, dto)` |
  | PUT | `api/Auth/change-password` | `[Authorize]` | `_auth.ChangePassword(userId, dto)` |
- **Lấy user id từ đâu:** `GetCurrentUserId()` đọc claim `user_id` trong JWT (dòng 96–103).
- **Xử lý lỗi:** bọc `try/catch`; đăng nhập sai trả `Unauthorized`, còn lại `BadRequest`.
- **Đụng DB:** gián tiếp qua `AuthService`.
- **Ai gọi:** app MAUI (`LoginPage`, `RegisterPage`, `ProfilePage`).

---

### A5. `SyncChain.API/Services/AuthService.cs`

- **Vai trò:** nghiệp vụ xác thực — băm/kiểm mật khẩu (BCrypt), tạo JWT, ghi audit.
- **Vì sao cần:** tách khỏi Controller để **giữ luật ở một chỗ** và tái dùng.
- **Phụ thuộc (DI):** `AppDbContext _db`, `IConfiguration _config` (đọc `Jwt:*`),
  `IAuditService _audit`, `ILogger<AuthService> _logger`.
- **Method chính:**
  - `Register(RegisterDTO)` — chuẩn hóa email, kiểm trùng, kiểm độ dài mật khẩu ≥6, lấy role
    `customer`, **băm BCrypt**, ghi user + audit **trong transaction**.
  - `Login(LoginDTO)` — tìm user theo email, `BCrypt.Verify`, kiểm `IsActive`; sai → ghi
    audit `LoginFailed` + ném lỗi; đúng → tạo JWT (claims `user_id`, name, email, **role**),
    hạn **2 giờ**, ký `HmacSha256`; ghi audit `Login` kèm device/location; trả `{ token, user }`.
  - `GetProfile(userId)` / `UpdateProfile(userId, dto)` — đọc/sửa hồ sơ; audit khi sửa.
  - `ChangePassword(userId, dto)` — kiểm mật khẩu cũ, đặt mật khẩu mới (băm), audit.
  - `ResolveRoleName(maVaiTro)` — tra tên role từ bảng `PhanQuyen` (tránh hardcode số).
- **Trích code tạo JWT (dòng 126–147):**
  ```csharp
  var claims = new[] {
      new Claim("user_id", user.MaNguoiDung.ToString()),
      new Claim(ClaimTypes.Name, user.TenDangNhap),
      new Claim(ClaimTypes.Email, user.Email),
      new Claim(ClaimTypes.Role, roleName)
  };
  var key   = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtSettings["Key"]!));
  var creds = new SigningCredentials(key, SecurityAlgorithms.HmacSha256);
  var token = new JwtSecurityToken(issuer: jwtSettings["Issuer"], audience: jwtSettings["Audience"],
                                   claims: claims, expires: DateTime.Now.AddHours(2),
                                   signingCredentials: creds);
  ```
- **Đụng DB:** có (đọc/ghi `NguoiDung`, `PhanQuyen`, `AuditLog`).
- **Ai gọi:** `AuthController`.

---

### A6. `SyncChain.API/Controllers/OrderController.cs`

- **Vai trò:** endpoint đơn hàng: tạo, xem danh sách/chi tiết, đổi trạng thái, khách tự hủy,
  theo dõi (tracking).
- **Phụ thuộc (DI):** `OrderService _service`, `AppDbContext _db` (các truy vấn đọc gọn được
  viết thẳng ở Controller).
- **Endpoint:**
  | Method | Route | Quyền | Ý nghĩa |
  |--------|-------|-------|---------|
  | POST | `api/Order` | `OrderWrite` | Tạo đơn. Nếu là nhân sự nội bộ, kiểm `SalesChannel` (không cho "Online"; chỉ "Cửa hàng"/"Facebook"). Nhận `Idempotency-Key` từ header nếu DTO trống. |
  | GET | `api/Order?status=` | `[Authorize]` | Nhân sự xem tất cả (lọc status); khách chỉ xem đơn của mình. |
  | GET | `api/Order/{id}` | `[Authorize]` | Chi tiết; khách chỉ xem đơn mình (`Forbid` nếu không). |
  | GET | `api/Order/full` | `OrderManage` | Toàn bộ đơn cho nhân sự. |
  | PUT | `api/Order/{id}/status` | `OrderManage` | Đổi trạng thái (DTO có `expectedStatus` + `concurrencyVersion`). |
  | PUT | `api/Order/{id}/cancel` | `[Authorize]` | Khách tự hủy đơn của mình. |
  | GET | `api/Order/{id}/tracking` | `[Authorize]` | Đơn + timeline + chi tiết + thanh toán gần nhất. |
- **Phân quyền dữ liệu:** `IsInternalRole(role)` (admin/manager/staff) quyết định thấy tất
  cả hay chỉ đơn của mình. `GetUserId()`/`GetRole()` đọc từ JWT.
- **Timeline:** `BuildTrackingTimeline(status)` sinh các bước `hoanThanh/hienTai/choDoi`
  (hoặc `huyBo`).
- **Đụng DB:** có (đọc trực tiếp; ghi ủy quyền cho Service).

---

### A7. `SyncChain.API/Services/OrderService.cs`  ⭐ trái tim nghiệp vụ

- **Vai trò:** toàn bộ logic đơn hàng phức tạp: **tạo đơn an toàn (idempotency + trừ kho +
  transaction)**, **đổi trạng thái an toàn (concurrency)**, hủy đơn, tự đẩy sang "processing"
  sau thanh toán, dọn giỏ khi thanh toán xong.
- **Phụ thuộc (DI):** `AppDbContext _db`, `InventoryService _inventory`, `IAuditService
  _audit`, `DeliveryEstimateService _deliveryEstimate`, `INotificationService _notify`.
- **Method chính:**
  - `CreateOrderAsync(userId, dto)` — điều phối: chuẩn hóa idempotency key → nếu đã có đơn
    ứng key thì **replay** (không tạo mới); validate & gộp item; kiểm dịch vụ "Hỏa tốc" chỉ
    cho nội thành; gọi `CreateOrderCoreAsync`; **bắt** các lỗi DB (unique violation của
    idempotency → replay; serialization/deadlock → `ConcurrencyConflictException`).
  - `CreateOrderCoreAsync(...)` (trong transaction): tải sản phẩm, kiểm tồn/ngừng bán, xác
    định người nhận (`ResolveRecipientAsync` — ưu tiên **sổ địa chỉ** `MaDiaChi` của khách),
    tạo `DonHang` trạng thái `pending`, **trừ kho** từng item qua
    `_inventory.DecreaseStockAsync(... OrderIssue, requireActiveProduct: true)`, tính tiền
    hàng, **tính phí ship phía server** cho đơn online (không tin client), lưu chi tiết,
    audit, commit. **KHÔNG** xóa giỏ ở bước này (chỉ xóa khi thanh toán xong).
  - `UpdateStatusAsync(orderId, request, legacyStatus, userId)` — **kiểm soát đồng thời**:
    lấy snapshot (status + version), kiểm hợp lệ, chặn nếu đã terminal, so `expectedStatus`
    + `expectedVersion`, kiểm `OrderStatuses.CanTransition`, rồi cập nhật **có điều kiện**
    bằng `ExecuteUpdateAsync` (WHERE status=expected AND version=expected) và **tăng version**.
    Nếu `cancel` thì **hoàn kho**. Audit + commit + đẩy realtime.
  - `CancelOwnOrderAsync(orderId, userId)` — khách chỉ hủy đơn **của mình** và **đang
    pending**; chặn nếu đã thanh toán online (yêu cầu liên hệ hoàn tiền); hoàn kho.
  - `AdvanceToProcessingAfterPaymentAsync(orderId)` — sau thanh toán online thành công, đẩy
    `pending → processing`; **idempotent** (đơn không còn pending thì bỏ qua).
  - `ClearPurchasedItemsAsync(orderId)` — xóa các sản phẩm đã mua khỏi giỏ; **chỉ** gọi khi
    thanh toán thành công (COD xác nhận / IPN online).
  - `RestoreCancelledOrderStockAsync` — cộng kho lại khi hủy (qua `IncreaseStockAsync`).
- **Trích code kiểm soát đồng thời (dòng 164–183):**
  ```csharp
  var changedRows = await _db.DonHang
      .Where(x => x.MaDonHang == orderId &&
                  x.TrangThai == expectedStatus &&
                  x.ConcurrencyVersion == expectedVersion)
      .ExecuteUpdateAsync(s => s
          .SetProperty(x => x.TrangThai, requestedStatus)
          .SetProperty(x => x.ConcurrencyVersion, x => x.ConcurrencyVersion + 1));
  if (changedRows != 1) { /* ai đó đã đổi trước → ném ConcurrencyConflictException */ }
  ```
  → Đây là **cách chống hai người đổi trạng thái cùng lúc**: chỉ đúng một người "thắng" vì
  điều kiện WHERE khớp version cũ chỉ đúng một lần.
- **Đụng DB:** rất nhiều (transaction).
- **Ai gọi:** `OrderController`, `PaymentController` (advance + clear cart), test scripts.

---

### A8. `SyncChain.API/Services/InventoryService.cs`  ⭐ chống bán lố

- **Vai trò:** thay đổi tồn kho **an toàn** và ghi **sổ cái giao dịch kho** (`GiaoDichKho`).
  Mọi thay đổi kho (đặt đơn, nhập, xuất, điều chỉnh, hoàn hủy) đi qua đây.
- **Phụ thuộc (DI):** `AppDbContext _db`, `IAuditService _audit`.
- **Method chính:**
  - `IncreaseStockAsync(...)` / `DecreaseStockAsync(...)` — kiểm loại giao dịch hợp lệ, chạy
    trong transaction, ủy quyền cho `ChangeStockCoreAsync`.
  - `AdjustStockAsync(dto, userId)` — điều chỉnh thủ công có lý do, ghi audit.
  - `GetCurrentStockAsync`, `GetTransactionHistoryAsync` — đọc tồn/lịch sử.
  - `ReconcileStockAsync(applyFix, userId)` — **đối soát** tồn hiện tại vs. tồn suy ra từ sổ
    cái, báo chênh lệch và (tùy chọn) đồng bộ.
  - `ChangeStockCoreAsync(...)` — **hạt nhân**: cập nhật `SoLuongTon` **có điều kiện** bằng
    `ExecuteUpdateAsync`, rồi ghi một dòng `GiaoDichKho` (tồn trước/sau).
- **Trích code trừ kho an toàn (dòng 337–347):**
  ```csharp
  changedRows = await _db.SanPham
      .Where(x => x.MaSanPham == productId &&
                  x.SoLuongTon >= quantity &&                       // chỉ trừ khi ĐỦ hàng
                  (!requireActiveProduct || x.TrangThai != "Ngung ban"))
      .ExecuteUpdateAsync(s => s
          .SetProperty(x => x.SoLuongTon, x => x.SoLuongTon - quantity)
          .SetProperty(x => x.TrangThai,  x => x.SoLuongTon - quantity <= 0 ? "Ngung ban" : x.TrangThai));
  if (changedRows != 1) { /* hết hàng / không đủ / ngừng bán → ném exception tương ứng */ }
  ```
  → Điều kiện `SoLuongTon >= quantity` ngay trong câu UPDATE khiến **không thể trừ âm**, kể
  cả khi nhiều request chạy song song. Đây là mấu chốt **chống oversell**.
- **Đụng DB:** có (transaction; cập nhật `SanPham`, ghi `GiaoDichKho`).
- **Ai gọi:** `OrderService`, `WarehouseReceiptService`, `WarehouseIssueService`,
  `InventoryController`.

---

### A9. `SyncChain.API/Controllers/PaymentController.cs`

- **Vai trò:** khởi tạo thanh toán (COD/VNPay/MoMo) và xử lý callback/IPN từ cổng thanh toán.
- **Phụ thuộc (DI):** `AppDbContext`, `VnPayService`, `MoMoService`, `INotificationService`,
  `EmailService`, `OrderService`.
- **Luồng chính (`POST api/payment/initiate`):**
  - Kiểm phương thức hợp lệ; khách chỉ trả cho đơn của mình; đơn phải `pending`; chặn thanh
    toán lặp (đã có giao dịch `Completed`).
  - **COD:** ghi `ThanhToan` = `Completed` ngay, **dọn giỏ** (`ClearPurchasedItemsAsync`),
    đẩy realtime + gửi email; đơn **giữ pending** để nhân sự xử lý.
  - **VNPay:** `_vnpay.CreatePaymentUrl(...)` → trả `paymentUrl` cho app mở trình duyệt.
  - **MoMo:** `_momo.CreatePaymentAsync(...)` → trả `paymentUrl`.
- **Callback/IPN:** `vnpay/return`, `vnpay/ipn`, `momo/return`, `momo/ipn` — **xác thực chữ
  ký**, cập nhật `ThanhToan`, nếu thành công thì `AdvanceToProcessingAfterPaymentAsync` +
  `ClearPurchasedItemsAsync` + email + đẩy realtime. `return` trả **trang HTML** đẹp để người
  dùng đóng lại và quay về app.
- **Bảo mật:** không tin resultCode từ redirect suông — VNPay dùng `ValidateCallback` (chữ
  ký), MoMo dùng `ValidateCallback(body)`.

---

### A10. `SyncChain.API/Services/NotificationService.cs`

- **Vai trò:** lưu thông báo vào DB (`ThongBao`) **và** đẩy realtime qua `OrderHub`.
- **Interface:** `INotificationService` (`SaveNotificationAsync`, `PushOrderStatusAsync`,
  `PushPaymentResultAsync`) — Controller/Service phụ thuộc **interface**, dễ thay/test.
- **Phụ thuộc (DI):** `IHubContext<OrderHub> _hub`, `AppDbContext _db`.
- **Trích code đẩy trạng thái (dòng 57–60):**
  ```csharp
  await _hub.Clients.Group($"user_{userId}").SendAsync("OrderStatusUpdated", orderId, status, ts);
  await _hub.Clients.Group("staff").SendAsync("OrderStatusUpdated", orderId, status, ts);
  ```
  → Gửi cho **chủ đơn** và nhóm **nhân sự**. Client bắt sự kiện `"OrderStatusUpdated"`.
- **Ai gọi:** `OrderService`, `PaymentController`.

---

### A11. `SyncChain.API/Hubs/OrderHub.cs` & `ChatHub.cs`

- **Vai trò:** điểm cuối SignalR. `OrderHub` cho đơn/thanh toán/thông báo; `ChatHub` cho chat.
- **`[Authorize]`:** chỉ client có JWT hợp lệ mới kết nối.
- **`OnConnectedAsync`:** tự thêm connection vào nhóm `user_<id>` (theo claim) và `staff`
  (nếu role nội bộ). Có thêm `JoinUserGroup`/`JoinStaffGroup` cho client gọi chủ động.
- **Vì sao dùng nhóm:** để **đẩy đúng người** — thông báo đơn của khách A không lọt sang
  khách B.

---

### A12. `SyncChain.API/Models/OrderStatuses.cs`  ⭐ nguồn chân lý trạng thái

- **Vai trò:** định nghĩa **một nơi duy nhất** bộ trạng thái đơn (`pending`, `processing`,
  `shipping`, `done`, `cancel`), tập `All`, `IsTerminal`, và **bảng chuyển hợp lệ**
  `CanTransition`.
- **Vì sao cần:** tránh mỗi nơi tự viết chuỗi trạng thái khác nhau (nguồn bug kinh điển).
- **Trích code:**
  ```csharp
  public static bool CanTransition(string cur, string req) => (cur, req) switch {
      (Pending, Processing) => true, (Pending, Cancel) => true,
      (Processing, Shipping) => true, (Processing, Cancel) => true,
      (Shipping, Done) => true, (Shipping, Cancel) => true,
      _ => false };
  ```
- **Ai dùng:** `OrderService`, `OrderController`, `PaymentController`, và client hiển thị qua
  `OrderStatusDisplay.cs`.

---

### A13. Các Model tiêu biểu

- **`NguoiDung.cs`** — user: `MaNguoiDung` (khóa), `TenDangNhap`, `MatKhauHash`, `Email`,
  `MaVaiTro`, `IsActive`, `Ho`/`Ten`/`SoDienThoai`. *Lưu ý:* chỉ lưu **hash**, không lưu mật
  khẩu thật.
- **`DonHang.cs`** — đơn: `TongTien`, `TrangThai` (mặc định `pending`), **`IdempotencyKey`**
  (chống trùng), **`ConcurrencyVersion`** (chống đồng thời), thông tin người nhận + giao hàng,
  `LoaiDichVu`, `PhuongThucThanhToan`, quan hệ `ChiTietDonHang` + `VanChuyen`.
- **`SanPham.cs`** — sản phẩm: `GiaBan`/`GiaNhap`, `SoLuongTon`, `TonKhoBanDau`, `MucTonThap`,
  `TrangThai` ("Hoat dong"/"Ngung ban"), `MaDanhMuc`.
- **`GiaoDichKho.cs`** — **sổ cái kho**: mỗi thay đổi tồn là một dòng (tồn trước/sau, loại,
  liên kết đơn/phiếu). Cho phép **đối soát**.
- Các model còn lại (`GioHang`, `ChiTietGioHang`, `DiaChi`, `ThongBao`, `ThanhToan`,
  `VanChuyen`, `LichSuVanChuyen`, `PhieuNhapKho`, `ChiTietPhieuNhap`, `PhieuXuatKho`,
  `ChiTietPhieuXuat`, `DanhMucSanPham`, `PhanQuyen`, `AuditLog`, `SystemErrorLog`, `Chat*`)
  ánh xạ 1-1 với bảng cùng tên; đọc `AppDbContext.OnModelCreating` để thấy quan hệ.

---

### A14. Các Service/Controller còn lại (tóm tắt vai trò)

| File | Vai trò |
|------|---------|
| `ProductService` / `ProductController` | CRUD sản phẩm, nhập tồn ban đầu, đổi trạng thái bán |
| `CartService` / `CartController` | Giỏ hàng của khách (thêm/sửa/xóa item) |
| `CategoryController` | Danh mục sản phẩm |
| `AddressService` / `AddressController` | Sổ địa chỉ giao hàng của khách |
| `ShippingService` / `ShippingController` | Tạo vận đơn, cập nhật trạng thái giao |
| `ShippingAutoCompletionService` | **Hosted service** chạy nền tự hoàn tất giao hàng đến hạn |
| `DeliveryEstimateService` | Ước tính phí ship + ngày giao (server tính, không tin client) |
| `WarehouseReceiptService/Controller` | Phiếu nhập kho |
| `WarehouseIssueService/Controller` | Phiếu xuất kho |
| `ChatService` / `ChatController` (+ `ChatHub`) | Chat nội bộ realtime, poll, ghim, reaction |
| `ReportController` | Doanh thu, thống kê (policy `ReportView`/`RevenueView`) |
| `AdminController` | Tạo/khóa/đặt lại mật khẩu nhân sự (policy `AdminOnly`) |
| `AuditService` / `AuditContextAccessor` / `AuditLogsController` | Ghi & xem nhật ký thao tác |
| `SystemErrorLogService` / `SystemErrorLogsController` | Ghi & xem nhật ký lỗi hệ thống |
| `EmailService` | Gửi email xác nhận đơn / kết quả thanh toán (SMTP) |
| `VnPayService` / `MoMoService` | Tạo URL thanh toán + xác thực chữ ký callback |
| `ApiExceptionHandler` + `Exceptions/ApiException.cs` | Biến exception → JSON `ApiErrorResponse` |

---

## PHẦN B — CLIENT (`app/SyncChain.Desktop`)

### B1. `MauiProgram.cs`
- **Vai trò:** cấu hình app MAUI: nạp **font**, đăng ký **HttpClient dùng chung** (lấy từ
  `ApiClientProvider.Client`), và (chỉ Windows) tinh chỉnh giao diện Picker/Entry/Editor.
- **Ai gọi:** runtime MAUI khi khởi động (`CreateMauiApp`).

### B2. `App.xaml.cs`
- **Vai trò:** điểm vào app. Giữ **một `SignalRService` tĩnh** (`App.SignalR`). Cửa sổ đầu là
  `LoginPage`. Cung cấp `ShowShell()` (nhân sự), `ShowCustomerShell()` (khách), `ShowLogin()`
  (đăng xuất: dừng SignalR + xóa session + về Login).
- **Vì sao có 2 shell:** phân tách giao diện theo vai trò để khách và nhân sự thấy menu khác
  nhau.

### B3. `Services/ApiClientProvider.cs`  ⭐
- **Vai trò:** **một `HttpClient` dùng chung** cho toàn app + quản lý **session** (token,
  role, userId) + kiểm `/health`.
- **Điểm quan trọng:**
  - `ApiBaseUrl` đọc từ env `SYNCCHAIN_API_URL`, mặc định `http://localhost:5292/`.
  - `SetSession(token, role, userId)` gắn header `Authorization: Bearer <token>` **một lần**
    cho `Client` → mọi request sau tự kèm token.
  - `IsBackendHealthyAsync()` gọi `/health`, **không bao giờ ném** (trả `false` khi lỗi).
- **Vì sao một HttpClient chung:** tránh cạn cổng (socket exhaustion) khi tạo nhiều
  `HttpClient`, và giữ token nhất quán.
- **Ai dùng:** mọi Page + `MauiProgram` + `SignalRService`.

### B4. `Services/SignalRService.cs`  ⭐
- **Vai trò:** kết nối `OrderHub` (`/hubs/order`) để nhận realtime; phát các event C#
  (`OnOrderStatusUpdated`, `OnPaymentResult`, `OnNewNotification`) cho Page đăng ký.
- **Điểm quan trọng:**
  - `StartAsync(token)` — dựng `HubConnection` với `AccessTokenProvider` (token qua query),
    `WithAutomaticReconnect()`; đăng ký handler; `JoinUserGroup` (+ `JoinStaffGroup` nếu nhân
    sự); mọi callback `BeginInvokeOnMainThread` để cập nhật UI đúng luồng.
- **Ai dùng:** các Page khách hàng (theo dõi đơn/thông báo), gọi từ luồng đăng nhập.

### B5. `Views/Pages/LoginPage.xaml.cs`  ⭐ ví dụ luồng client
- **Vai trò:** màn hình đăng nhập, hai cổng (nhân sự / khách).
- **Luồng:** `OnAppearing` gọi `IsBackendHealthyAsync()` (báo thân thiện nếu server chưa
  sẵn); `LoginAsync(customerShell)` gọi `POST api/Auth/login` kèm device/location, kiểm role
  đúng cổng, `SetSession`, rồi `ShowShell`/`ShowCustomerShell`. Ghi log `[Desktop/Login]`.
- **Ai gọi:** app khi mở, và nút Đăng nhập.

### B6. Các Service/Page client khác (tóm tắt)
- `Services/SessionGuard.cs` — xử lý **401 tập trung**: khi API trả 401 → xóa session → về
  Login (tránh lặp code khắp nơi).
- `Services/OrderStatusDisplay.cs` — map `pending/processing/...` → chữ tiếng Việt + màu
  (client hiển thị, đồng bộ với `OrderStatuses` phía server).
- `Services/AppLog.cs` — log `[Desktop/<khu vực>]` (Info/Warn/Error).
- `Models/AppModels.cs`, `Models/CustomerApiModels.cs` — lớp đọc JSON trả về (VD
  `LoginResponseApi`).
- `Views/Pages/*` — mỗi màn hình chức năng; xử lý gọi API + binding dữ liệu (Products, Cart,
  CreateOrder, Payment, CustomerOrders, OrderTracking, Address, Profile, Notification,
  Dashboard, Imports, Logs, Chat, UserAccess...).

---

## PHẦN C — PROTOTYPE (`src/`, tùy chọn)

- `src/server.js` — API Express (cổng 3000) của bản prototype.
- `src/index.js` — script thử tầng DB: tạo bảng từ `database/TaoBang.sql`, thêm sản phẩm,
  **đặt hàng an toàn bằng transaction** (`UPDATE ... WHERE SoLuongTon >= $1`) — cùng ý tưởng
  chống oversell như bản .NET nhưng viết SQL tay.
- `src/db.js` — pool kết nối `pg`. `src/constants/statusEnum.js` — enum trạng thái (bản Node).
- `src/middleware/validateStatusTransition.js` — kiểm chuyển trạng thái (bản Node).

> Nhắc lại: phần C **không** thuộc đường chạy của app MAUI; chỉ để tham khảo/đối chiếu.

---

➡️ Tiếp theo: [07_Thu_vien.md](07_Thu_vien.md) — từng thư viện: là gì, dùng ở đâu, không có
thì phải tự viết gì.
