# Chương 4 — Kiến trúc hệ thống

## 4.1. Bức tranh tổng thể

```
┌──────────────────────────────────────────────────────────────────────┐
│                     SyncChain.Desktop  (client .NET MAUI, Windows)     │
│                                                                        │
│   ┌───────────┐         ┌────────────────────────────────────────┐    │
│   │  AppShell │         │  Views/Pages/*.xaml (+ .xaml.cs)        │    │
│   │(nội bộ)   │         │  Login, Products, Cart, Orders, Chat... │    │
│   ├───────────┤         └────────────────────────────────────────┘    │
│   │CustomerSh.│         ┌────────────────────────────────────────┐    │
│   │(khách)    │         │  Services/                              │    │
│   └───────────┘         │  ApiClientProvider (HttpClient chung)   │    │
│                         │  SignalRService  · SessionGuard         │    │
│                         └────────────────────────────────────────┘    │
└───────────────┬─────────────────────────────────────┬────────────────┘
                │ HTTP + JWT Bearer (REST/JSON)        │ SignalR (WebSocket + JWT)
                ▼                                       ▼
┌──────────────────────────────────────────────────────────────────────┐
│                    SyncChain.API  (ASP.NET Core .NET 10)  :5292        │
│                                                                        │
│  Middleware pipeline: [HTTP log] → [Exception] → [AuthN] → [AuthZ]     │
│                                                                        │
│   Controllers/  ── nhận request, phân quyền, gọi Service               │
│        │                                                               │
│        ▼                                                               │
│   Services/     ── NGHIỆP VỤ: transaction, luật, tính toán             │
│        │                    (OrderService, InventoryService, Auth...)  │
│        ▼                                                               │
│   Data/AppDbContext (EF Core)  ── phiên dịch C# ↔ SQL                  │
│        │                                                               │
│   Hubs/ (SignalR)  ◄── Services đẩy sự kiện realtime ──►               │
└────────────────────────────────┬─────────────────────────────────────┘
                                  │ Npgsql (SSL)
                                  ▼
                       ┌────────────────────────┐
                       │   PostgreSQL (Neon /    │
                       │   local)  DB: syncchain │
                       └────────────────────────┘

   (Tùy chọn, độc lập)  ui/ (HTML) → src/ Express :3000 → cùng PostgreSQL
```

## 4.2. Luồng một request đi từ đầu đến cuối (rất quan trọng)

Lấy ví dụ **khách bấm "Đăng nhập"**. Đây là chuỗi đầy đủ, từng tầng, có tên file thật:

```
[1] Người dùng bấm nút Đăng nhập
        │  LoginPage.xaml.cs → OnLoginClicked → LoginAsync()
        ▼
[2] MAUI validate (email/mật khẩu trống?) rồi gọi HttpClient
        │  _http.PostAsJsonAsync("api/Auth/login", { email, password, device, location })
        ▼   (HTTP POST, body JSON, chưa cần token)
[3] Request tới backend, đi qua middleware pipeline (Program.cs)
        │  [HTTP log] → [Exception handler] → (login là public, không cần AuthN)
        ▼
[4] AuthController.Login([FromBody] LoginDTO dto)     ← Controller
        │  gọi _auth.Login(dto)
        ▼
[5] AuthService.Login(dto)                            ← Service (nghiệp vụ)
        │  - _db.NguoiDung.FirstOrDefault(email)      ← EF Core query
        │  - BCrypt.Verify(password, hash)            ← kiểm mật khẩu
        │  - kiểm IsActive
        │  - tạo JWT (claims: user_id, role, ...) ký bằng Jwt:Key
        │  - _audit.AddSuccess(Login...)  → ghi AuditLog
        ▼
[6] EF Core dịch sang SQL, chạy trên PostgreSQL, trả dữ liệu user
        ▼
[7] Trả JSON { token, user { MaNguoiDung, TenDangNhap, Email, role } }
        │  ngược lên Controller → Ok(result) → HTTP 200
        ▼
[8] MAUI nhận token:
        │  ApiClientProvider.SetSession(token, role, userId)   ← gắn Bearer cho mọi request sau
        │  App.ShowShell() / App.ShowCustomerShell()           ← mở giao diện theo vai trò
        ▼
[9] (Nền) SignalRService.StartAsync(token) → mở WebSocket tới /hubs/order,
        JoinUserGroup (và JoinStaffGroup nếu là nhân sự) để nhận realtime.
```

Chiều **realtime ngược lại** (server chủ động đẩy):
```
Nhân viên đổi trạng thái đơn
   → OrderService.UpdateStatusAsync (transaction + ConcurrencyVersion)
   → NotificationService.PushOrderStatusAsync
       → lưu ThongBao vào DB
       → _hub.Clients.Group("user_<id>").SendAsync("OrderStatusUpdated", ...)   [OrderHub]
   → WebSocket đẩy xuống app khách
   → SignalRService._conn.On("OrderStatusUpdated", ...) → cập nhật UI ngay
```

## 4.3. Vì sao lại chia nhiều tầng như vậy?

| Tầng | Trách nhiệm | Nếu gộp hết vào một chỗ thì sao |
|------|-------------|--------------------------------|
| **View/Page (MAUI)** | Hiển thị, bắt sự kiện bấm nút | — |
| **ApiClientProvider** | Một `HttpClient` chung + gắn token | Mỗi page tự tạo HttpClient → rối, token không đồng bộ |
| **Controller** | Nhận HTTP, đọc claim, phân quyền, gọi Service | Nếu nhồi nghiệp vụ vào Controller → khó test, khó tái dùng |
| **Service** | Nghiệp vụ: transaction, luật, tính tiền, đẩy realtime | Nếu để Controller làm → lặp code giữa các endpoint |
| **DbContext (EF)** | Truy cập DB an toàn kiểu C# | Nếu viết SQL tay khắp nơi → dễ SQL injection, khó bảo trì |
| **PostgreSQL** | Lưu trữ bền vững | — |

Nguyên tắc: **mỗi tầng chỉ gọi tầng ngay dưới nó**. Controller không đụng thẳng DB cho các
nghiệp vụ phức tạp (nó ủy quyền cho Service). Điều này giúp **thay đổi một tầng không phá vỡ
các tầng khác** — xem [10_Tu_duy_thiet_ke.md](10_Tu_duy_thiet_ke.md).

## 4.4. Điểm đặc biệt của backend SyncChain

1. **Tự dựng schema + seed khi khởi động** (`Program.cs`): `EnsureCreated()` + loạt
   `CREATE TABLE IF NOT EXISTS` / `ALTER TABLE ... ADD COLUMN IF NOT EXISTS` để bắt kịp các
   cột/bảng mới, rồi seed 4 role + tài khoản admin. → *Chạy phát là có DB dùng ngay*, không
   cần migration tay.
2. **Health check ẩn danh `/health`:** trả 200 khi DB nối được, 503 khi không. App gọi
   trước khi cho đăng nhập (`LoginPage.OnAppearing`), để báo lỗi thân thiện thay vì crash.
3. **Xử lý lỗi tập trung:** `ApiExceptionHandler` + `UseExceptionHandler` biến exception
   thành JSON `ApiErrorResponse` chuẩn (`{ code, message, details, traceId }`).
4. **Logging có cấu trúc:** `[Startup]`, `[HTTP]`, `[Auth]` — dễ theo dõi khi demo.
5. **Phân quyền bằng policy theo role** (khai ở `Program.cs`), gắn lên endpoint bằng
   `[Authorize(Policy = "...")]`.

## 4.5. Các "đường trục" dữ liệu quan trọng cần nhớ

- **Chuỗi kết nối:** `.env` (`DATABASE_URL`) → `EnvFileLoader.Load()` → biến môi trường →
  `EnvFileLoader.ToPostgreSqlConnectionString` → `AddDbContext(UseNpgsql(...))`.
- **Danh tính:** đăng nhập → JWT → header `Authorization` (HTTP) hoặc `?access_token=`
  (SignalR) → middleware → `User.FindFirst("user_id")` / `ClaimTypes.Role` trong Controller.
- **Realtime:** Service → `IHubContext<OrderHub>` → nhóm `user_<id>` / `staff` → client
  `SignalRService`.
- **Trạng thái đơn:** **một nguồn chân lý** duy nhất là `OrderStatuses.cs` (chuỗi lowercase
  `pending/processing/shipping/done/cancel`), client hiển thị qua `OrderStatusDisplay.cs`.

---

➡️ Tiếp theo: [05_Folder_Guide.md](05_Folder_Guide.md) — mổ xẻ từng thư mục: vì sao cần,
chứa gì, xóa đi thì sao.
