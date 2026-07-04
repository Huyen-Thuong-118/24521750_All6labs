# Chương 7 — Giải thích từng thư viện

Chỉ liệt kê **thư viện dự án thật sự dùng** (căn cứ `SyncChain.API.csproj`,
`SyncChain.Desktop.csproj`, `package.json`). Với mỗi thư viện: *là gì → dùng ở đâu → không
có thì phải tự viết gì → ưu/nhược*.

> ⚠️ **Nói thẳng để khỏi hiểu lầm:** dự án **KHÔNG** dùng AutoMapper, Serilog,
> FluentValidation, MediatR, Newtonsoft.Json, ASP.NET Identity. Đây là những thư viện *phổ
> biến* thường thấy trong các dự án .NET khác, nhưng SyncChain chọn cách tối giản (xem mục
> 7.9). Nếu bạn thấy tài liệu mẫu nhắc tới chúng, đừng đi tìm trong mã — không có.

---

## Backend — NuGet (`SyncChain.API.csproj`)

### 7.1. `Npgsql.EntityFrameworkCore.PostgreSQL` (10.0.2) + Entity Framework Core
- **Là gì:** **EF Core** là ORM (phiên dịch C# ↔ SQL); **Npgsql provider** dạy EF Core nói
  chuyện với **PostgreSQL** cụ thể.
- **Dùng ở đâu:** `AppDbContext` (khai `DbSet<>`), mọi Service/Controller truy vấn DB, `Program.cs`
  (`UseNpgsql`, `EnsureCreated`). Các cột `jsonb`, `ExecuteUpdateAsync`, transaction đều là
  tính năng EF Core/Npgsql.
- **Không có thì:** phải tự mở kết nối, viết SQL tay, tự map kết quả sang object (như
  `src/index.js` bản Node). Tốn công và dễ lỗi/SQL injection.
- **Ưu:** năng suất cao, an toàn kiểu, transaction dễ. **Nhược:** trừu tượng có thể sinh SQL
  không tối ưu nếu dùng ẩu; cần hiểu tracking/no-tracking.

### 7.2. `Microsoft.EntityFrameworkCore.Tools` (10.0.7)
- **Là gì:** bộ lệnh `dotnet ef` để tạo/áp dụng **migration**.
- **Dùng ở đâu:** tạo các file trong `SyncChain.API/Migrations/`. *Chạy* dự án không cần
  (dùng `EnsureCreated`), nhưng cần khi phát triển thay đổi schema có kiểm soát.
- **Không có thì:** phải sửa DB thủ công bằng SQL — mất khả năng "phiên bản hóa" schema.

### 7.3. `Microsoft.AspNetCore.Authentication.JwtBearer` (10.0.7)
- **Là gì:** middleware **đọc & kiểm JWT** trong header `Authorization: Bearer ...`.
- **Dùng ở đâu:** `Program.cs` (`AddJwtBearer` cấu hình `TokenValidationParameters`,
  `OnMessageReceived` cho SignalR). Việc **tạo** token dùng `System.IdentityModel.Tokens.Jwt`
  trong `AuthService`.
- **Không có thì:** phải tự parse token, tự kiểm chữ ký/hạn dùng ở **mỗi** endpoint — vừa
  lặp, vừa dễ sai bảo mật.
- **Ưu:** chuẩn hóa xác thực stateless. **Nhược:** token đã cấp không thu hồi được trước hạn.

### 7.4. `BCrypt.Net-Next` (4.1.0)
- **Là gì:** thư viện **băm mật khẩu** BCrypt (có salt, cố tình chậm).
- **Dùng ở đâu:** `AuthService.Register` (`HashPassword`), `AuthService.Login` +
  `ChangePassword` (`Verify`), `Program.cs` (băm mật khẩu admin seed).
- **Không có thì:** phải tự cài thuật toán băm an toàn — rất dễ làm sai (lưu mật khẩu thô,
  hoặc dùng MD5/SHA yếu, thiếu salt).
- **Ưu:** an toàn, đơn giản. **Nhược:** chậm có chủ đích (đó là tính năng, không phải lỗi).

### 7.5. `Swashbuckle.AspNetCore` (6.5.0) — Swagger
- **Là gì:** sinh **tài liệu API tương tác** tại `/swagger` từ chính mã Controller.
- **Dùng ở đâu:** `Program.cs` (`AddSwaggerGen` + nút nhập `Bearer`, `UseSwagger/UI`).
- **Không có thì:** phải viết tài liệu API tay và test bằng Postman/curl. Swagger cho **thử
  API ngay trên trình duyệt**, tiện demo/chấm điểm.
- **Ưu:** tài liệu luôn khớp code. **Nhược:** nên tắt/ẩn ở production thật.

### 7.6. `Microsoft.AspNetCore.SignalR` (đi kèm ASP.NET Core)
- **Là gì:** thư viện **realtime hai chiều** (WebSocket + fallback), khái niệm **Hub** và
  **Group**.
- **Dùng ở đâu:** `AddSignalR`, `MapHub<ChatHub>`/`MapHub<OrderHub>`, `IHubContext<OrderHub>`
  trong `NotificationService`.
- **Không có thì:** phải tự dựng WebSocket, tự quản kết nối/nhóm/broadcast, tự lo reconnect.
- **Ưu:** đẩy đúng người theo nhóm, tự chọn transport. **Nhược:** cần xử lý xác thực token
  qua query cho WebSocket (đã làm ở `Program.cs`).

---

## Client — NuGet (`SyncChain.Desktop.csproj`)

### 7.7. `Microsoft.Maui.Controls` + `Microsoft.Maui.Graphics`
- **Là gì:** **.NET MAUI** — framework dựng giao diện đa nền tảng bằng **XAML** + C#;
  `Graphics` để vẽ (biểu đồ dashboard qua `IDrawable`).
- **Dùng ở đâu:** toàn bộ `Views/Pages/*.xaml`, `AppShell`/`CustomerShell`, `Views/Charts/*`.
- **Không có thì:** phải chọn framework UI khác (WPF/WinForms/Avalonia). MAUI cho một codebase
  chạy được nhiều nền tảng (đồ án dùng Windows).

### 7.8. `Microsoft.AspNetCore.SignalR.Client`
- **Là gì:** phía **client** của SignalR (`HubConnectionBuilder`, `HubConnection`).
- **Dùng ở đâu:** `Services/SignalRService.cs` (kết nối `/hubs/order`, lắng nghe sự kiện).
- **Không có thì:** phải tự viết client WebSocket + giao thức của SignalR.

> `HttpClient` (gọi REST) và `System.Text.Json` (đọc/ghi JSON) là **thư viện chuẩn của .NET**,
> không phải NuGet ngoài. `LoginPage` dùng `PostAsJsonAsync`/`ReadFromJsonAsync` từ
> `System.Net.Http.Json`.

---

## 7.9. Vì sao KHÔNG dùng các thư viện "quen mặt"?

| Thư viện phổ biến | Vai trò thường thấy | SyncChain thay bằng gì |
|-------------------|--------------------|------------------------|
| **AutoMapper** | Map Entity ↔ DTO tự động | Map **thủ công** trong Controller/Service (`Select(x => new {...})`) — rõ ràng, dễ debug cho quy mô đồ án |
| **FluentValidation** | Validate dạng fluent | Data annotations trên DTO + kiểm tay trong Service + `InvalidModelStateResponseFactory` |
| **Serilog** | Logging nâng cao ra file/sink | `ILogger` mặc định của .NET với prefix `[HTTP]`/`[Auth]`/`[Startup]` |
| **MediatR** | Tách request/handler (CQRS) | Gọi Service trực tiếp — đơn giản, đủ dùng |
| **Newtonsoft.Json** | Serialize JSON | `System.Text.Json` (chuẩn .NET, nhanh) |
| **ASP.NET Identity** | Khung quản lý user/role đầy đủ | Tự quản `NguoiDung`/`PhanQuyen` + JWT thủ công — kiểm soát rõ, hợp mục tiêu học tập |

**Triết lý:** với một đồ án môn học, **ít phụ thuộc = dễ hiểu, dễ giải thích khi bảo vệ, dễ
kiểm soát**. Mọi bước (băm mật khẩu, tạo token, map dữ liệu) đều **hiện rõ trong mã** thay vì
bị giấu sau thư viện. Đánh đổi là viết tay nhiều hơn một chút.

---

## Prototype — npm (`package.json`, tùy chọn)

- **`express` (5)** — web framework Node cho API prototype.
- **`pg`** — driver PostgreSQL cho Node (mở pool, `pool.query`).
- **`cors`** — cho phép trình duyệt (trang `ui/`) gọi API khác cổng.
- **`dotenv`** — đọc `.env` (cùng `DATABASE_URL`).

---

➡️ Tiếp theo: [08_Xay_dung_tu_dau.md](08_Xay_dung_tu_dau.md) — dựng lại dự án từ số 0, từng
bước, giải thích **tại sao** làm bước đó.
