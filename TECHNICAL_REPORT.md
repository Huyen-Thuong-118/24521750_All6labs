# BÁO CÁO KỸ THUẬT — DỰ ÁN NT106_SyncChain

> Tài liệu kỹ thuật dành cho người mới tiếp quản project. **Mọi kết luận dựa trên source code thực tế.** Chỗ nào không có trong source được ghi rõ **"Không tìm thấy trong source"**.
>
> Phạm vi: `SyncChain.API` (backend ASP.NET Core) + `app/SyncChain.Desktop` (client .NET MAUI). Thư mục `src/` (Node/Express) và `ui/` (HTML) là **prototype cũ, không phải đường chạy của app MAUI** (client chỉ gọi `http://localhost:5292` = `SyncChain.API`, xem `app/SyncChain.Desktop/Services/ApiClientProvider.cs:7-8`).

---

## MỤC LỤC

1. [Tổng quan](#1-tổng-quan)
2. [Công nghệ sử dụng](#2-công-nghệ-sử-dụng)
3. [Cấu trúc thư mục](#3-cấu-trúc-thư-mục)
4. [Kiến trúc hệ thống](#4-kiến-trúc-hệ-thống)
5. [Danh sách tính năng](#5-danh-sách-tính-năng)
6. [Phân tích chi tiết từng tính năng](#6-phân-tích-chi-tiết-từng-tính-năng)
7. [Phân tích Component (client)](#7-phân-tích-component-client)
8. [State Management](#8-state-management)
9. [Routing](#9-routing)
10. [API Layer (toàn bộ endpoint)](#10-api-layer-toàn-bộ-endpoint)
11. [Database](#11-database)
12. [Authentication](#12-authentication)
13. [Security](#13-security)
14. [Design Pattern](#14-design-pattern)
15. [Thư viện sử dụng](#15-thư-viện-sử-dụng)
16. [Những điểm cần chú ý](#16-những-điểm-cần-chú-ý)
17. [Đánh giá chất lượng](#17-đánh-giá-chất-lượng)
18. [Báo cáo tiến độ](#18-báo-cáo-tiến-độ)
19. [Hướng dẫn cài đặt & chạy dự án (từ số 0 đến hoàn thiện)](#19-hướng-dẫn-cài-đặt--chạy-dự-án-từ-số-0-đến-hoàn-thiện)

---

## 1. Tổng quan

**SyncChain** là hệ thống **client–server quản lý vận hành chuỗi bán lẻ** (đồ án môn NT106 – Lập trình mạng căn bản). Hệ thống có 2 cổng người dùng:

- **Cổng khách hàng (`CustomerShell`)**: xem sản phẩm → giỏ hàng → đặt hàng → thanh toán (COD/VNPay/MoMo) → theo dõi đơn; quản lý địa chỉ, hồ sơ, thông báo.
- **Cổng nội bộ (`AppShell`)** cho `staff`/`manager`/`admin`: quản lý sản phẩm, danh mục, kho (nhập/xuất/kiểm kê), đơn hàng, vận chuyển, báo cáo/dashboard, chat nội bộ, nhật ký hệ thống, phân quyền người dùng.

Trọng tâm "lập trình mạng": **nhiều client đồng thời**, **realtime push qua SignalR**, và **xử lý xung đột** (transaction + cột `ConcurrencyVersion`).

---

## 2. Công nghệ sử dụng

### Backend — `SyncChain.API/SyncChain.API.csproj`
| Công nghệ | Version | Vai trò | Vì sao dùng |
|---|---|---|---|
| ASP.NET Core Web API | **.NET 10** | REST + SignalR | Framework backend chuẩn của .NET |
| `Npgsql.EntityFrameworkCore.PostgreSQL` | 10.0.2 | ORM (EF Core) cho PostgreSQL | Truy vấn type-safe, migration, tránh viết SQL tay |
| `Microsoft.AspNetCore.Authentication.JwtBearer` | 10.0.7 | Xác thực JWT | Stateless auth cho nhiều client |
| `BCrypt.Net-Next` | 4.1.0 | Hash mật khẩu | Hash 1 chiều + salt chống lộ mật khẩu |
| `Swashbuckle.AspNetCore` | 6.5.0 | Swagger/OpenAPI | Tài liệu + test API |
| `Microsoft.EntityFrameworkCore.Tools` | 10.0.7 | Migration EF Core | Sinh/áp schema |
| SignalR (built-in) | — | Realtime WebSocket | Đẩy cập nhật đơn/thanh toán/thông báo/chat |

### Client — `app/SyncChain.Desktop/SyncChain.Desktop.csproj`
| Công nghệ | Version | Vai trò |
|---|---|---|
| **.NET MAUI** (`Microsoft.Maui.Controls`) | `net10.0-windows10.0.19041.0` | UI desktop (Windows) |
| `Microsoft.AspNetCore.SignalR.Client` | 10.0.7 | Nhận realtime từ backend |
| `Microsoft.Extensions.Logging.Debug` | 9.0.8 | Log debug |

### Database
- **PostgreSQL** (local hoặc Neon cloud). Chuỗi kết nối đọc từ `DATABASE_URL` trong `.env` gốc qua `SyncChain.API/Configuration/EnvFileLoader.cs`, không dùng `appsettings.ConnectionStrings` (xem `Program.cs:61-68`).

---

## 3. Cấu trúc thư mục

```
NT106_SyncChain/
├── SyncChain.API/                 ← BACKEND (ASP.NET Core .NET 10) — đường chạy chính
│   ├── Program.cs                    # Bootstrap: DI, JWT, SignalR, seed DB, middleware
│   ├── Controllers/                  # 18 controller REST (tầng nhận HTTP)
│   ├── Services/                     # 19 service (tầng nghiệp vụ)
│   ├── Data/AppDbContext.cs          # EF Core DbContext (26 DbSet)
│   ├── Models/                       # 31 entity + hằng số (OrderStatuses, ShippingStatuses...)
│   ├── DTOs/                         # Data Transfer Objects (request/response)
│   ├── Migrations/                   # 6 migration EF Core + snapshot
│   ├── Hubs/                         # SignalR: ChatHub, OrderHub
│   ├── ExceptionHandling/            # ApiExceptionHandler (xử lý lỗi tập trung)
│   ├── Exceptions/                   # ApiException + exception nghiệp vụ
│   ├── Configuration/EnvFileLoader.cs# Nạp .env → connection string
│   ├── appsettings.json              # JWT, VnPay, MoMo, Email
│   └── appsettings.Development.json  # Override cho dev (MoMo sandbox...)
│
├── app/SyncChain.Desktop/         ← CLIENT (.NET MAUI)
│   ├── MauiProgram.cs                # Bootstrap MAUI (fonts, HttpClient)
│   ├── App.xaml.cs                   # Điều phối shell: ShowLogin / ShowShell / ShowCustomerShell
│   ├── AppShell.xaml(.cs)            # Shell NỘI BỘ (admin/manager/staff)
│   ├── CustomerShell.xaml(.cs)       # Shell KHÁCH HÀNG
│   ├── Views/Pages/                  # 21 trang (.xaml + .xaml.cs)
│   ├── Views/Charts/                 # Drawable vẽ biểu đồ dashboard
│   ├── Models/                       # AppModels.cs, CustomerApiModels.cs (deserialize API)
│   ├── Services/                     # ApiClientProvider, SignalRService, SessionGuard...
│   ├── Converters/                   # IValueConverter cho XAML binding
│   └── Resources/                    # Fonts, Styles, Colors, Images
│
├── src/                           ← (LEGACY) Node/Express prototype — KHÔNG dùng cho MAUI
├── ui/                            ← (LEGACY) HTML prototype
├── database/                      # Script SQL tham khảo
├── scripts/                       # run-database/backend/frontend/all (.ps1)
├── .postman/                      # Collection test API
└── .env / .env.example            # DATABASE_URL
```

**Ý nghĩa các folder backend:**
- `Controllers/` — nhận HTTP, đọc JWT claim (`user_id`, `role`), phân quyền `[Authorize(Policy=...)]`, gọi Service.
- `Services/` — business logic: transaction, validate, tính toán, gọi EF Core.
- `Data/AppDbContext.cs` — ánh xạ entity ↔ bảng PostgreSQL.
- `DTOs/` — định dạng dữ liệu vào/ra (không lộ entity DB).
- `Hubs/` — SignalR đẩy realtime.

**Ý nghĩa các folder client:**
- `Views/Pages/` — mỗi màn hình = `*.xaml` (UI) + `*.xaml.cs` (gọi API).
- `Services/ApiClientProvider.cs` — `HttpClient` dùng chung + lưu JWT + base URL.
- `Services/SignalRService.cs` — kết nối hub, phát event realtime.
- `Models/AppModels.cs`, `CustomerApiModels.cs` — POCO để `System.Text.Json` deserialize JSON.

---

## 4. Kiến trúc hệ thống

```mermaid
flowchart TD
    subgraph Client["CLIENT — .NET MAUI (Windows)"]
        UI["Views/Pages (XAML + code-behind)"]
        SR["SignalRService (client)"]
        API["ApiClientProvider (HttpClient + JWT)"]
        UI --> API
        UI --- SR
    end
    API -- "REST + Bearer JWT" --> CTRL
    SR -- "WebSocket (?access_token=)" --> HUBS
    subgraph Server["SERVER — ASP.NET Core .NET 10"]
        MW["Middleware: JWT auth, ExceptionHandler, HTTP log"]
        CTRL["Controllers (18)"]
        SVC["Services (19) — business logic"]
        HUBS["Hubs: ChatHub, OrderHub"]
        EF["AppDbContext (EF Core)"]
        MW -.-> CTRL
        CTRL --> SVC
        SVC --> EF
        SVC --> HUBS
    end
    EF -- "Npgsql" --> DB[("PostgreSQL (26 bảng)")]
    SVC -- "tạo link + verify callback" --> PAY["VNPay / MoMo sandbox"]
    PAY -- "return / IPN" --> CTRL
```

**Phân tầng:** Controller → Service → EF Core (`AppDbContext`) → PostgreSQL. **Không có tầng Repository riêng** — `AppDbContext` (Unit-of-Work + Repository của EF Core) là tầng truy cập dữ liệu.

1. **MAUI Pages**: người dùng thao tác → code-behind gọi API.
2. **ApiClientProvider**: gắn header `Authorization: Bearer <JWT>` (`ApiClientProvider.cs:26-27`).
3. **Controllers**: đọc `user_id`/`role` từ JWT, phân quyền, gọi Service.
4. **Services**: transaction, validate, tính toán, EF Core.
5. **Hubs (SignalR)**: đẩy realtime về client.
6. **Cổng thanh toán**: server tạo link, cổng gọi ngược `return`/`ipn`.

---

## 5. Danh sách tính năng

### A. Khách hàng (CustomerShell)
| # | Tính năng | Endpoint chính | Trang MAUI |
|---|---|---|---|
| 1 | Đăng ký + tự đăng nhập | `POST /api/auth/register`,`/login` | `RegisterPage` |
| 2 | Đăng nhập (JWT) | `POST /api/auth/login` | `LoginPage` |
| 3 | Trang chủ khách | `/api/auth/profile`,`/api/order`,`/api/product` | `CustomerHomePage` |
| 4 | Danh sách SP + lọc + phân trang | `GET /api/product` | `ProductsPage` |
| 5 | Chi tiết SP (khách) | `GET /api/product/{id}/public-detail` | `ProductDetailPage` |
| 6 | Giỏ hàng | `/api/cart*` | `CartPage`, `ProductDetailPage` |
| 7 | Quản lý địa chỉ | `/api/address*` | `AddressPage` |
| 8 | Đặt hàng | `POST /api/order` | `CartPage` |
| 9 | Thanh toán COD/VNPay/MoMo | `/api/payment/*` | `PaymentPage` |
| 10 | Theo dõi đơn (realtime) | `GET /api/order/{id}/tracking` | `OrderTrackingPage` |
| 11 | Đơn của tôi | `GET /api/order` | `CustomerOrdersPage` |
| 12 | Chi tiết đơn + tự hủy | `GET /api/order/{id}`, `PUT /api/order/{id}/cancel` | `OrderDetailPage` |
| 13 | Thông báo | `/api/notification*` | `NotificationPage` |
| 14 | Hồ sơ + đổi mật khẩu | `/api/auth/profile`,`/change-password` | `ProfilePage` |

### B. Nội bộ (AppShell — staff/manager/admin)
| # | Tính năng | Controller |
|---|---|---|
| 15 | Dashboard/báo cáo | `ReportController` |
| 16 | Quản lý sản phẩm | `ProductController` |
| 17 | Quản lý danh mục | `CategoryController` |
| 18 | Nhập kho (phiếu nhập + duyệt) | `WarehouseReceiptsController` |
| 19 | Xuất kho (phiếu xuất + duyệt) | `WarehouseIssuesController` |
| 20 | Điều chỉnh/kiểm kê tồn kho | `InventoryController` |
| 21 | Quản lý đơn nội bộ | `OrderController` |
| 22 | Tạo đơn nội bộ (cửa hàng/Facebook) | `OrderController` |
| 23 | Vận chuyển | `ShippingController` |
| 24 | Chat nội bộ (realtime, poll, gọi) | `ChatController` + `ChatHub` |
| 25 | Nhật ký kiểm toán | `AuditLogsController` |
| 26 | Nhật ký lỗi hệ thống | `SystemErrorLogsController` |
| 27 | Quản lý người dùng / phân quyền | `AdminController` |

### C. Xuyên suốt
Xác thực & phân quyền JWT; Realtime SignalR; Xử lý lỗi tập trung; Health check `/health`; Logging có cấu trúc; Idempotency + Concurrency; Audit log.

---

## 6. Phân tích chi tiết từng tính năng

### 6.1 Đăng ký tài khoản khách hàng

**Mục đích:** Tạo tài khoản `customer` mới rồi tự đăng nhập luôn.

**Công nghệ:** ASP.NET Core Controller, EF Core (transaction), BCrypt (hash mật khẩu), JWT (login sau khi tạo). Client: MAUI code-behind + `HttpClient` + `System.Text.Json`.

**File liên quan:**
- `app/SyncChain.Desktop/Views/Pages/RegisterPage.xaml.cs` — form + validate client + gọi API.
- `SyncChain.API/Controllers/AuthController.cs:20-32` — action `Register`.
- `SyncChain.API/Services/AuthService.cs:27-80` — logic tạo user.
- `SyncChain.API/DTOs/RegisterDTO.cs` — dữ liệu đăng ký.

**Luồng xử lý:**
```
Người dùng nhập form (RegisterPage)
→ validate client (email/mật khẩu ≥6, khớp confirm, đồng ý điều khoản)  RegisterPage.xaml.cs:46-65
→ POST /api/auth/register { email, password, ho, ten, tenDangNhap }     RegisterPage.xaml.cs:75-83
→ AuthController.Register → AuthService.Register                          AuthController.cs:25
→ kiểm tra email trùng, hash BCrypt, tạo NguoiDung role=customer         AuthService.cs:36-64
→ lưu DB trong transaction + ghi audit                                   AuthService.cs:66-76
→ (client) tự POST /api/auth/login lấy JWT                               RegisterPage.xaml.cs:97-103
→ SetSession(token, role, userId) + mở CustomerShell                     RegisterPage.xaml.cs:127-132
```

**Function quan trọng:**
- `AuthService.Register(RegisterDTO)` — validate + hash + tạo user + audit.
- `RegisterPage.OnRegisterClicked` — điều phối 2 bước (register → auto login).

**Phân tích code** (`AuthService.cs:54-76`):
```csharp
var user = new NguoiDung {
    Email = email,
    TenDangNhap = tenDangNhap,                              // ưu tiên client gửi; trống → phần trước @
    MatKhauHash = BCrypt.Net.BCrypt.HashPassword(password), // hash 1 chiều
    MaVaiTro = role.MaVaiTro,                               // role "customer"
    IsActive = true, Ho = ..., Ten = ..., SoDienThoai = ...
};
using var transaction = _db.Database.BeginTransaction();
_db.NguoiDung.Add(user); _db.SaveChanges();                // ghi user (lấy được MaNguoiDung)
_audit.AddSuccess(AuditActions.Create, "NguoiDung", ...);  // ghi nhật ký kiểm toán
_db.SaveChanges(); transaction.Commit();
```
Từng bước: hash mật khẩu → tạo entity với role customer → lưu trong transaction → ghi audit → commit.

**API:** `POST /api/auth/register` — Body `{ email, password, ho, ten, tenDangNhap, soDienThoai }` → Response `"Dang ky thanh cong"` (string). **Register KHÔNG trả token** → client phải login riêng.

**Database:** ghi bảng `NguoiDung` (Email, TenDangNhap, MatKhauHash, MaVaiTro=1, IsActive, Ho, Ten, SoDienThoai) + `AuditLog`.

**Tóm tắt:** Đăng ký tạo tài khoản `customer` với mật khẩu hash BCrypt trong transaction có audit; endpoint chỉ trả chuỗi thông báo nên client tự gọi login để lấy JWT rồi vào cổng khách. Validate 2 lớp (client + server). Không lộ mật khẩu (chỉ lưu hash).

---

### 6.2 Đăng nhập & JWT

**Mục đích:** Xác thực và cấp JWT để các API sau phân quyền.

**File liên quan:**
- `app/SyncChain.Desktop/Views/Pages/LoginPage.xaml.cs` — form + gọi API + chọn cổng.
- `SyncChain.API/Controllers/AuthController.cs:35-47` — action `Login`.
- `SyncChain.API/Services/AuthService.cs:83-173` — verify + sinh JWT.
- `SyncChain.API/Program.cs:111-143` — cấu hình `JwtBearer`.
- `app/SyncChain.Desktop/Services/ApiClientProvider.cs` — lưu token.

**Luồng xử lý:**
```
Nhập email/password (LoginPage)
→ POST /api/auth/login { email, password, device, location }   LoginPage.xaml.cs:83-89
→ AuthService.Login: tìm user, BCrypt.Verify                    AuthService.cs:92-106
→ kiểm tra IsActive (tài khoản bị khóa → chặn)                  AuthService.cs:108-118
→ tạo JWT với claims user_id, name, email, role (hết hạn 2h)    AuthService.cs:126-146
→ trả { token, user{ MaNguoiDung, TenDangNhap, Email, role } }  AuthService.cs:162-172
→ client: kiểm tra role đúng cổng (customer vs admin/staff)     LoginPage.xaml.cs:107-118
→ SetSession(token, role, userId) → mở shell tương ứng          LoginPage.xaml.cs:120-125
```

**Phân tích code — sinh JWT** (`AuthService.cs:126-146`):
```csharp
var claims = new[] {
    new Claim("user_id", user.MaNguoiDung.ToString()),   // ID để phân quyền dữ liệu
    new Claim(ClaimTypes.Name,  user.TenDangNhap),
    new Claim(ClaimTypes.Email, user.Email),
    new Claim(ClaimTypes.Role,  roleName)                // role để RequireRole
};
var key   = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtSettings["Key"]!));
var creds = new SigningCredentials(key, SecurityAlgorithms.HmacSha256);
var token = new JwtSecurityToken(issuer, audience, claims,
                expires: DateTime.Now.AddHours(2), signingCredentials: creds);
```
- Claim `user_id` là **chìa khóa cô lập dữ liệu**: mọi controller đọc claim này để chỉ trả dữ liệu của chính user.
- Ký HMAC-SHA256 bằng khóa bí mật trong `appsettings.json:11` (`Jwt:Key`).
- Hết hạn 2 giờ.

**Phía client — lưu session** (`ApiClientProvider.cs:21-28`):
```csharp
public static void SetSession(string token, string? role, int? userId = null) {
    Token = token; Role = role; UserId = userId;
    Client.DefaultRequestHeaders.Authorization =
        new AuthenticationHeaderValue("Bearer", token);   // gắn token cho MỌI request sau
}
```

**API:** `POST /api/auth/login` — Body `{ email, password, device?, location? }` → Response `{ token, user{ MaNguoiDung, TenDangNhap, Email, role } }`. Sai thông tin → 401.

**Cơ chế:** JWT stateless. Server không lưu session; mỗi request client gửi token, middleware `JwtBearer` (`Program.cs:118-143`) verify chữ ký + issuer + audience + hạn dùng, dựng `ClaimsPrincipal`. Phân quyền bằng `[Authorize(Policy=...)]` dựa trên claim role.

**Tóm tắt:** Login verify mật khẩu bằng BCrypt, chặn tài khoản khóa, sinh JWT chứa `user_id` + `role` ký HMAC-SHA256 hết hạn 2h. Client lưu token vào `HttpClient` dùng chung và mở đúng shell theo role. `device`/`location` được ghi audit để hiển thị lịch sử đăng nhập. Không refresh token (xem [Điểm cần chú ý](#16-những-điểm-cần-chú-ý)).

---

### 6.3 Hồ sơ cá nhân & đổi mật khẩu

**File:** `ProfilePage.xaml.cs`; `AuthController.cs:50-93` (`Profile`, `UpdateProfile`, `ChangePassword`); `AuthService.cs:176-254`.

**Luồng:**
- `GET /api/auth/profile` → `AuthService.GetProfile(userId)` đọc `NguoiDung` theo `user_id` claim → trả `{ MaNguoiDung, TenDangNhap, Email, Ho, Ten, SoDienThoai, role, IsActive }`.
- `PUT /api/auth/profile` body `{ Username, Ho, Ten, SoDienThoai }` → cập nhật (chỉ ghi field không rỗng) — `AuthService.UpdateProfile:197-221`. **Lưu ý contract:** DTO là `UpdateProfileDTO.Username` (không phải `TenDangNhap`).
- `PUT /api/auth/change-password` body `{ CurrentPassword, NewPassword }` → verify mật khẩu cũ bằng BCrypt rồi hash mật khẩu mới — `AuthService.ChangePassword:230-254`.

**Cơ chế bảo vệ:** cả 3 endpoint `[Authorize]`, lấy `userId` từ claim `user_id` (`AuthController.GetCurrentUserId:96-103`) → chỉ thao tác trên hồ sơ của chính người gọi. Email đăng nhập **không** đổi được (UpdateProfileDTO không có Email), client khóa ô email (`ProfilePage.xaml.cs:36`).

**Tóm tắt:** Hồ sơ đọc/ghi theo `user_id` trong JWT; đổi mật khẩu yêu cầu mật khẩu hiện tại; email cố định. Mọi thay đổi ghi audit.

---

### 6.4 Sản phẩm & danh mục

**Mục đích:** Xem/tìm/lọc sản phẩm (khách + nội bộ); CRUD sản phẩm/danh mục (manager/admin).

**File:** `ProductController.cs`, `ProductService.cs`, `CategoryController.cs`; client `ProductsPage.xaml.cs`, `ProductDetailPage.xaml.cs`, `ProductFormPage.xaml.cs`.

**Endpoint & phân quyền:**
- `GET /api/product` (Policy `ProductRead` = customer+staff+manager+admin) — trả toàn bộ SP kèm hiệu suất bán tháng này/trước (`ProductService.GetAll:23-71`).
- `GET /api/product/{id}` (ProductRead) — SP cơ bản.
- `GET /api/product/{id}/detail` (Policy `StaffOrAbove`) — chi tiết **nội bộ** (giá nhập, doanh thu, lịch sử kho, hiệu suất) — `ProductService.GetDetail:250-318`.
- `GET /api/product/{id}/public-detail` (ProductRead) — chi tiết **an toàn cho khách** (ẩn giá nhập/doanh thu) — `ProductService.GetPublicDetail`.
- `POST/PUT/DELETE /api/product` (Policy `ProductWrite` = manager/admin).
- `POST /api/product/{id}/import` (Policy `InventoryWrite`).
- Danh mục: `GET /api/category` (Authorize), CRUD `ManagerOrAdmin` (`CategoryController.cs`).

**Luồng lọc + phân trang (client, `ProductsPage.xaml.cs`):**
```
OnAppearing → GET /api/product → _allProducts                 ProductsPage.xaml.cs:57
→ BuildCategoryFilters (nhóm theo danh mục)                    :120-149
→ FilteredProducts (lọc theo danh mục chọn)                    :28-33
→ ApplyPagination (PageSize=20, Skip/Take)                     :78-96
```
Lọc + phân trang **thực hiện ở client** trên danh sách đã tải; danh mục chỉ được nhóm trên dữ liệu có sẵn.

**Phân tích code — hiệu suất bán (`ProductService.GetAll:42-70`):** truy vấn `ChiTietDonHang` các đơn **không hủy** trong khoảng [tháng trước, tháng này], group theo sản phẩm, tính `CurrentMonth`/`PreviousMonth` → `ToProductResponse` tính `%` tăng trưởng.

**Chi tiết khách vs nội bộ:** `ProductDetailPage.LoadProductDetailAsync` chọn endpoint theo role — khách gọi endpoint an toàn, nội bộ gọi `/detail` đầy đủ; XAML ẩn "Giá nhập"/"Hiệu suất" với khách qua binding `CanManageProducts`.

**Tóm tắt:** Sản phẩm dùng chung bảng `SanPham`; khách và nội bộ khác nhau ở endpoint chi tiết (ẩn/hiện giá vốn). Lọc/phân trang ở client. Danh mục xóa mềm khi còn sản phẩm (`CategoryController.Delete:131-153`).

---

### 6.5 Giỏ hàng

**Mục đích:** Mỗi khách có 1 giỏ; thêm/sửa/xóa sản phẩm, tính tổng.

**File:** `CartController.cs`, `CartService.cs`; client `CartPage.xaml.cs`, `ProductDetailPage.OnAddToCartClicked`.

**API (tất cả `[Authorize]`, lấy `user_id` từ JWT):**
| Method | Route | Ý nghĩa |
|---|---|---|
| GET | `/api/cart` | Lấy giỏ + tổng |
| POST | `/api/cart/items` `{ MaSanPham, SoLuong }` | Thêm |
| PUT | `/api/cart/items/{maSanPham}?soLuong=` | Sửa số lượng |
| DELETE | `/api/cart/items/{maSanPham}` | Xóa 1 SP |
| DELETE | `/api/cart` | Xóa cả giỏ |

**Phân tích code — thêm (`CartService.AddItem:38-75`):**
```csharp
if (dto.SoLuong <= 0) throw ...;                          // chặn số lượng ≤ 0
var product = _db.SanPham.Find(dto.MaSanPham) ?? throw KeyNotFound;
if (product.TrangThai == "Ngung ban") throw ...;         // chặn SP ngừng bán
var cart = GetOrCreateCart(userId);                       // tự tạo giỏ nếu chưa có
var existing = ...FirstOrDefault(... MaSanPham ...);
var newQty = (existing?.SoLuong ?? 0) + dto.SoLuong;      // cộng dồn nếu đã có
if (newQty > product.SoLuongTon) throw ...;              // chặn vượt tồn kho
```
Cô lập theo user: `GetOrCreateCart(userId)` (`CartService.cs:120-129`) luôn tìm/tạo giỏ theo `MaNguoiDung`.

**Tính tổng (`CartService.GetCart:14-36`):** `ThanhTien = GiaBan × SoLuong`, `TongTien = Σ ThanhTien` — **tính đúng 1 lần** (không nhân sai).

**Tóm tắt:** Giỏ hàng cô lập theo `user_id`, tự tạo khi cần, validate tồn kho + trạng thái SP, cộng dồn số lượng. Tổng tính chuẩn giá×số lượng.

---

### 6.6 Quản lý địa chỉ giao hàng

**File:** `AddressController.cs`, `AddressService.cs`; client `AddressPage.xaml.cs`.

**API (`[Authorize]`, theo `user_id`):** `GET /api/address`, `POST /api/address`, `PUT /api/address/{id}`, `DELETE /api/address/{id}`, `PUT /api/address/{id}/default`.

**Cơ chế:** mỗi địa chỉ gắn `MaNguoiDung`; "đặt mặc định" (`LaMacDinh`) dùng khi đặt hàng — server nạp thông tin người nhận từ `MaDiaChi` thay vì tin dữ liệu client. Bảng `DiaChi` (xem [Database](#11-database)).

**Tóm tắt:** CRUD địa chỉ cô lập theo user + cờ mặc định; là nguồn thông tin người nhận cho đơn khách tự đặt.

---

### 6.7 Đặt hàng (Idempotency + Concurrency + Tồn kho)

**Mục đích:** Tạo đơn từ giỏ (khách) hoặc nhập tay (nội bộ), trừ tồn kho an toàn, chống tạo trùng.

**File:** `OrderController.cs:28-66` (`CreateOrder`), `OrderService.cs` (`CreateOrderAsync`, `CreateOrderCoreAsync`), `DTOs/CreateOrderDTO.cs`; client `CartPage.OnCheckoutClicked`, `CreateOrderPage` (nội bộ).

**Luồng (khách):**
```
CartPage: chọn địa chỉ → POST /api/order { Items[], MaDiaChi }   CartPage.xaml.cs:100-104
→ OrderController.CreateOrder (Policy OrderWrite)                  OrderController.cs:28-30
→ OrderService.CreateOrderAsync:                                   OrderService.cs:33-80
   • NormalizeIdempotencyKey → nếu key đã dùng → trả đơn cũ (replay)
   • ValidateAndMergeItems (gộp trùng, chặn qty ≤ 0)
   • CreateOrderCoreAsync trong transaction:                       OrderService.cs:392-530
       - load sản phẩm, kiểm tồn kho/trạng thái
       - ResolveRecipientAsync: nạp người nhận từ MaDiaChi (của chính user)
       - tạo DonHang (pending) + trừ kho qua InventoryService.DecreaseStockAsync
       - tính subtotal + phí ship (DeliveryEstimateService cho đơn online)
       - ghi ChiTietDonHang, audit, commit
→ trả { MaDonHang, Subtotal, ShippingFee, TongTien }
→ client điều hướng PaymentPage
```

**Idempotency (`OrderService.cs:37-43, 592-614`):** nếu `IdempotencyKey` (hoặc header `Idempotency-Key`) đã tồn tại → trả lại đơn đã tạo (`BuildReplayResult`), tránh double-submit tạo 2 đơn. Nếu key trùng do race → bắt `DbUpdateException` unique-violation → trả đơn cũ (`OrderService.cs:62-71`).

**Concurrency tồn kho:** trừ kho qua `InventoryService.DecreaseStockAsync(... requireActiveProduct: true)`; nếu xung đột serialization/deadlock → `ConcurrencyConflictException` (`OrderService.cs:72-79`).

**Người nhận an toàn (`ResolveRecipientAsync:535-567`):** khi có `MaDiaChi`, server truy `DiaChi` theo `MaDiaChi && MaNguoiDung == userId` — **không tin địa chỉ client gửi**; sai → `ValidationApiException`.

**Phân quyền kênh bán (`OrderController.cs:42-63`):** role nội bộ không được tạo đơn `Online` (phải để khách tự đặt); đơn `Facebook` bắt buộc đủ địa chỉ.

**API:** `POST /api/order` — Body `{ Items:[{MaSanPham,SoLuong}], MaDiaChi?, SalesChannel?, ... }` → `{ Message, MaDonHang, Subtotal, ShippingFee, TongTien, IsReplay }`.

**Tóm tắt:** Tạo đơn là điểm nóng "lập trình mạng": transaction + trừ kho + idempotency (chống trùng) + concurrency (chống race tồn kho). Đơn khách luôn ở trạng thái `pending` chờ thanh toán; thông tin người nhận lấy từ sổ địa chỉ của chính user (không tin client). Kết quả trả tách `Subtotal`/`ShippingFee`/`TongTien`.

---

### 6.8 Ước tính giao hàng (phí ship + ETA)

**File:** `ShippingController.cs:31-33` (`POST /api/shipping/estimate`), `DeliveryEstimateService.cs`.

**Cơ chế (`DeliveryEstimateService.Estimate:27-71`):**
- Phân loại khu vực (`ClassifyArea`): Hải đảo / Vùng núi / Nội thành đô thị lớn / Ngoại thành / Huyện nông thôn.
- Ước tính khoảng cách (`EstimateDistance`) + số ngày vận chuyển (`TransitDays` theo dịch vụ + khu vực + cân nặng).
- **Phí ship** = `deliveryDays × 10.000đ`, **miễn phí nếu đơn ≥ 500.000đ** (`DeliveryEstimateService.cs:41-43, 7-8`).
- ETA sớm/muộn (`AddBusinessDays` bỏ T7/CN), độ tin cậy theo khu vực.

**Tóm tắt:** Phí ship và ETA suy ra từ tỉnh/thành + phường/xã + loại dịch vụ + cân nặng; phí = số ngày × 10k, miễn phí đơn lớn. Là nguồn phí ship cho đơn online (server tự tính, không tin client).

---

### 6.9 Thanh toán (COD / VNPay / MoMo)

**Mục đích:** Chốt phương thức và ghi nhận thanh toán; đơn online tự chuyển `processing` khi trả tiền thành công.

**File:** `PaymentController.cs`, `VnPayService.cs`, `MoMoService.cs`; client `PaymentPage.xaml.cs`; cấu hình `appsettings.json:29-44`.

**API:**
| Method | Route | Ý nghĩa |
|---|---|---|
| POST | `/api/payment/initiate` `{ MaDonHang, PhuongThuc }` (Policy OrderWrite) | Khởi tạo (cod/vnpay/momo) |
| GET | `/api/payment/vnpay/return` | Trình duyệt redirect về (HTML kết quả) |
| GET/POST | `/api/payment/vnpay/ipn` | VNPay server callback |
| GET | `/api/payment/momo/return` | MoMo redirect về |
| POST | `/api/payment/momo/ipn` | MoMo server callback |
| GET | `/api/payment/status/{orderId}` (Authorize) | Xem trạng thái thanh toán |

**Luồng COD (`PaymentController.cs:82-104`):**
```
initiate {cod} → kiểm đơn tồn tại + của chính khách + đang pending + chưa completed
→ tạo ThanhToan (Completed) → SaveChanges
→ ClearPurchasedItemsAsync(orderId): xóa các SP của đơn khỏi giỏ (chỉ khi thành công)
→ PushPaymentResultAsync + gửi email → trả { message, orderId }
```

**Luồng VNPay/MoMo:**
```
initiate {vnpay|momo} → tạo ThanhToan (Pending) + link cổng → client mở trình duyệt
→ user trả tiền trên cổng → cổng gọi return (trình duyệt) + IPN (server-to-server)
→ ValidateCallback (verify HMAC-SHA256) → ParseCallback (resultCode)
→ HandleVnPayResult/HandleMoMoResult: cập nhật ThanhToan (Completed/Failed)
   nếu success: AdvanceToProcessingAfterPaymentAsync + ClearPurchasedItemsAsync + email + notify
```

**Phân tích code — chữ ký MoMo create (`MoMoService.cs:34-40`):**
```csharp
var rawSignature =
  $"accessKey={accessKey}&amount={(long)amount}&extraData={extraData}" +
  $"&ipnUrl={ipnUrl}&orderId={momoOrderId}&orderInfo={orderInfo}" +
  $"&partnerCode={partnerCode}&redirectUrl={redirectUrl}" +
  $"&requestId={requestId}&requestType={requestType}";
var signature = HmacSha256(secretKey, rawSignature);  // đúng thứ tự field alphabet của MoMo v2
```
Thứ tự field ký đúng chuẩn MoMo v2. Có logging `resultCode`/response để chẩn đoán (`MoMoService.cs`).

**Cơ chế xóa giỏ đúng thời điểm:** giỏ **chỉ** được xóa khi thanh toán thành công (COD confirm / VNPay-MoMo IPN success) qua `OrderService.ClearPurchasedItemsAsync`; **không** xóa lúc tạo đơn — để khách thoát giữa chừng vẫn còn giỏ (`OrderService.CreateOrderCoreAsync` đã bỏ lệnh xóa lúc tạo).

**Database:** bảng `ThanhToan` (MaDonHang, PhuongThuc, TrangThaiThanhToan, SoTien, MaGiaoDich, DuLieuCallback, NgayTao/NgayCapNhat).

**Tóm tắt:** COD chốt ngay (Completed); VNPay/MoMo tạo link → verify callback HMAC-SHA256 → cập nhật trạng thái + đẩy đơn sang `processing` + realtime + email. Chống thanh toán trùng (chặn nếu đã Completed). Giỏ xóa đúng lúc thành công.

> **Lưu ý vận hành:** `RedirectUrl`/`IpnUrl` mặc định là `localhost` → IPN server-to-server của cổng **không gọi về localhost được** (cần ngrok/domain public). Credential MoMo cần là sandbox thật (xem `appsettings.Development.json`).

---

### 6.10 Theo dõi đơn + Realtime (SignalR)

**File:** `OrderController.cs:262-360` (`GetTracking`, `BuildTrackingTimeline`); `Hubs/OrderHub.cs`; `Services/NotificationService.cs`; client `OrderTrackingPage.xaml.cs`, `Services/SignalRService.cs`.

**Luồng:**
```
GET /api/order/{id}/tracking → kiểm quyền (khách chỉ xem đơn của mình)   OrderController.cs:269-274
→ trả { order, payment, chiTiet, timeline }                              OrderController.cs:310-327
→ client render timeline + đăng ký OnOrderStatusUpdated (SignalR)         OrderTrackingPage.xaml.cs:32
→ khi nhân sự đổi trạng thái → NotificationService.PushOrderStatusAsync   NotificationService.cs:40-61
   → hub.Clients.Group("user_{id}").SendAsync("OrderStatusUpdated", ...)
→ client nhận event → reload tracking                                     OrderTrackingPage.xaml.cs:46-50
```

**OrderHub (`Hubs/OrderHub.cs`):** khi client kết nối, thêm vào group `user_{userId}` (nhận thông báo cá nhân) và group `staff` nếu là nội bộ. Client kết nối với `?access_token=` (JWT truyền qua query cho WebSocket — `Program.cs:133-139`).

**Timeline (`OrderController.BuildTrackingTimeline:331-360`):** sinh các bước `pending→processing→shipping→done` với trạng thái `hoanThanh/hienTai/choDoi`; đơn hủy → 2 bước `pending(hoanThanh) + cancel(huyBo)`.

**Tóm tắt:** Theo dõi đơn trả timeline + chi tiết + thanh toán gần nhất, cô lập theo chủ đơn. Realtime qua SignalR OrderHub: đổi trạng thái đẩy tức thì tới đúng khách + nhóm staff.

---

### 6.11 Vận chuyển (nội bộ)

**File:** `ShippingController.cs`, `ShippingService.cs`, `Models/VanChuyen.cs`, `LichSuVanChuyen.cs`, `ShippingStatuses.cs`.

**API (Policy `OrderManage` cho ghi):**
- `POST /api/orders/{orderId}/shipping` — tạo vận đơn.
- `PUT /api/orders/{orderId}/shipping` — cập nhật.
- `PUT /api/orders/{orderId}/shipping/status` — đổi trạng thái giao (có `expectedStatus` + `concurrencyVersion`).
- `GET /api/orders/{orderId}/shipping`, `/history`, `GET /api/shipping/tracking/{trackingNumber}` — xem (khách chỉ xem đơn của mình, `ShippingController.cs:91-97`).

**Cơ chế:** trạng thái giao hàng dùng bộ `ShippingStatuses` (Pending/Ready/PickedUp/InTransit/Delivered/Failed/Returned/Cancelled); mỗi lần đổi ghi `LichSuVanChuyen`. Có `ShippingAutoCompletionService` (hosted service, `Program.cs:207`) tự hoàn tất giao hàng theo thời gian.

**Tóm tắt:** Nhân sự tạo/cập nhật vận đơn có concurrency-check; khách chỉ xem vận đơn của đơn mình. Trạng thái giao hàng riêng biệt với trạng thái đơn.

---

### 6.12 Thông báo

**File:** `NotificationController.cs`, `NotificationService.cs`; client `NotificationPage.xaml.cs`.

**API (`[Authorize]`, theo `user_id`):** `GET /api/notification`, `GET /api/notification/unread-count`, `PUT /api/notification/{id}/read`, `PUT /api/notification/read-all`.

**Cơ chế:** `NotificationService.SaveNotificationAsync` ghi bảng `ThongBao` (loại `order_status`/`payment_result`), đồng thời `PushOrderStatusAsync`/`PushPaymentResultAsync` đẩy realtime qua OrderHub. Client bind + đánh dấu đã đọc; tap thông báo có `MaDonHang` → mở trang theo dõi đơn.

**Tóm tắt:** Thông báo lưu DB + đẩy realtime, cô lập theo user. Hai loại chính: cập nhật trạng thái đơn và kết quả thanh toán.

---

### 6.13 Chat nội bộ (realtime, poll, gọi, file)

**File:** `ChatController.cs`, `ChatService.cs`, `Hubs/ChatHub.cs`; entity `ChatConversation/ChatMessageEntity/ChatParticipant/ChatPoll*`; client `ChatPage.xaml.cs`.

**Phạm vi:** toàn bộ `[Authorize(Policy="InternalOnly")]` — **chỉ nội bộ** (staff/manager/admin).

**API tiêu biểu (`ChatController.cs`):** `GET /api/chat/users`, `conversations` (list/create), `groups` (tạo nhóm), `conversations/{id}/messages`, `POST /api/chat/messages`, `attachments` (upload file), `messages/{id}/pin|recall|reaction`, `polls` + `poll/vote|options|lock`, `calls/log`, `conversations/{id}/read`.

**Realtime (`ChatHub.cs`):** mỗi user vào group `user:{id}`; gửi tin → `ChatController.PushMessageAsync` bắn `MessageReceived` tới tất cả participant. Hub còn xử lý **tín hiệu gọi** (StartCall/AcceptCall/RejectCall/EndCall/Busy/SendCallSignal) — phục vụ gọi audio/video signaling.

**Tính năng nâng cao:** ghim/thu hồi/reaction tin nhắn, poll (bình chọn, thêm lựa chọn, khóa), upload file lưu `wwwroot/uploads/chat` (`ChatController.cs:78-103`), log cuộc gọi.

**Tóm tắt:** Chat nội bộ đầy đủ (nhóm, ghim, thu hồi, reaction, poll, file, signaling gọi) qua REST + SignalR ChatHub, giới hạn cho nhân sự.

---

### 6.14 Kho: nhập / xuất / điều chỉnh / kiểm kê

**File:** `WarehouseReceiptsController.cs`, `WarehouseIssuesController.cs`, `InventoryController.cs` + service tương ứng; entity `PhieuNhapKho/ChiTietPhieuNhap`, `PhieuXuatKho/ChiTietPhieuXuat`, `GiaoDichKho`.

**Nhập kho (`/api/warehouse-receipts`):** vòng đời có **duyệt** — `POST` (tạo, InventoryWrite) → `PUT {id}/submit` → `PUT {id}/approve` (InventoryApprove) → `PUT {id}/complete` → hoặc `cancel`. Đọc cần InventoryRead.

**Xuất kho (`/api/warehouse-issues`):** tương tự — tạo/submit (InventoryWrite) → complete (InventoryApprove) → cancel; có `GET history`.

**Điều chỉnh/kiểm kê (`/api/inventory`):** `GET products/{id}` (tồn hiện tại), `GET transactions` (lịch sử `GiaoDichKho`), `POST adjustments` (InventoryWrite), `POST reconcile` (kiểm kê, `ApplyFix=true` cần InventoryApprove — `InventoryController.cs:56-76`).

**Cơ chế tồn kho:** mọi thay đổi tồn đi qua `InventoryService` và ghi `GiaoDichKho` (loại theo `InventoryTransactionTypes`), đảm bảo truy vết. Tạo đơn/hủy đơn cũng tạo giao dịch kho (`OrderService`).

**Tóm tắt:** Kho có quy trình phiếu nhập/xuất kèm duyệt (phân quyền write vs approve), điều chỉnh và kiểm kê; mọi biến động tồn đều ghi `GiaoDichKho` để audit.

---

### 6.15 Báo cáo & Dashboard

**File:** `ReportController.cs`; client `DashboardPage.xaml.cs`, `Views/Charts/*`.

**API (Policy `ReportView` = manager/admin; doanh thu `RevenueView`):**
- `GET /api/reports/dashboard` — tổng hợp đơn/doanh thu/tồn kho/vận chuyển theo khoảng ngày.
- `GET /api/reports/categories`, `/inventory`, `/shipping`, `/orders`, `/top-products`, `/logs`.
- `GET /api/reports/revenue` (RevenueView) — doanh thu theo day/month/year; `/revenue-by-date`.

**Phân tích code — doanh thu thuần (`ReportController.cs:41-56`):** chỉ tính đơn `done`: `Σ(SoLuong × DonGia)`; `cancelledValue` cho đơn hủy; `shippingFee` từ `VanChuyen` của đơn done.

**Cơ chế:** truy vấn EF Core `AsNoTracking` + group; chuẩn hóa khoảng ngày UTC (`NormalizeRange`); biểu đồ vẽ bằng `IDrawable` (`OrderTrendChartDrawable`, `InventoryDonutDrawable`).

**Tóm tắt:** Bộ báo cáo phong phú (dashboard, danh mục, tồn kho, vận chuyển, đơn, top SP, doanh thu theo kỳ) cho manager/admin; doanh thu chỉ tính đơn hoàn tất.

---

### 6.16 Quản trị người dùng & phân quyền

**File:** `AdminController.cs` (toàn bộ `[Authorize(Policy="AdminOnly")]`).

**API:** `GET /api/admin/users`, `GET users/{id}`, `GET users/{id}/login-history`, `POST users`/`create-user`/`create-manager`/`create-staff`, `PUT users/{id}`, `PUT users/{id}/active` (khóa/mở), `PUT users/{id}/password` (reset), `DELETE users/{id}` (xóa mềm).

**Cơ chế an toàn:** chỉ quản lý **tài khoản nội bộ** (admin/manager/staff — `IsInternalRole`); chặn admin **tự khóa/xóa chính mình** (`AdminController.cs:200-201, 245-246, 306-307`); xóa là **soft-delete** (`IsActive=false`) để giữ dữ liệu liên quan. Mọi hành động ghi audit (Create/Update/RoleChange/StatusChange/PasswordChange/Delete).

**Lịch sử đăng nhập (`GetLoginHistory:54-91`):** đọc `AuditLog` các bản ghi `Login`/`Success` của user → hiển thị thiết bị/vị trí/IP (từ metadata `device`/`location` ghi lúc login).

**Tóm tắt:** Admin CRUD tài khoản nội bộ với nhiều rào an toàn (không tự khóa, soft-delete, chỉ nội bộ), reset mật khẩu, xem lịch sử đăng nhập; toàn bộ audit.

---

### 6.17 Nhật ký kiểm toán & lỗi hệ thống

**File:** `AuditLogsController.cs` (Policy `AuditRead` = admin), `SystemErrorLogsController.cs` (Policy `SystemErrorLogRead` = admin), `Services/AuditService.cs`, `Services/SystemErrorLogService.cs`; client `LogsPage.xaml.cs`.

**Audit log:** `GET /api/audit-logs` (lọc theo user/role/action/entity/result/traceId/khoảng ngày, phân trang), `GET /api/audit-logs/{id}`. Bảng `AuditLog` lưu Before/After/Metadata/IP/UserAgent/TraceId. `AuditService` được gọi khắp các service khi có hành động quan trọng.

**System error log:** `GET /api/system-error-logs` — ghi lỗi validation/500/401/403 tập trung (đăng ký singleton `ISystemErrorLogService`, gọi trong `Program.cs` khi model-state invalid `:33-41` và status 401/403 `:475-487`).

**Tóm tắt:** Hai kênh nhật ký cho admin: audit (hành động nghiệp vụ, có before/after) và system error (lỗi kỹ thuật). Hỗ trợ điều tra và truy vết bằng TraceId.

---

## 7. Phân tích Component (client)

**Trang (ContentPage) tiêu biểu:**
- `CustomerHomePage` — hub khách: tải song song profile + sản phẩm + đơn (`Task.WhenAll`), bind `UserName/UserEmail/Metrics/RecentOrders/SuggestedProducts`. State giữ trong chính page (property + `OnPropertyChanged`).
- `ProductsPage` — danh sách + filter danh mục + phân trang client (`PageButtons`, `FilteredProducts`).
- `ProductDetailPage` — chi tiết + khu mua hàng (khách) / quản trị (nội bộ), phân nhánh endpoint theo role.
- `CartPage` — giỏ + checkout (chọn địa chỉ → tạo đơn → PaymentPage).
- `PaymentPage` — chọn COD/VNPay/MoMo, hiển thị breakdown Tạm tính/Phí ship/Tổng, chờ SignalR cho thanh toán online.
- `OrderTrackingPage` / `OrderDetailPage` — theo dõi + chi tiết đơn, dựng timeline động.
- `ChatPage` — chat realtime (nội bộ).
- `DashboardPage` — báo cáo + biểu đồ (`Views/Charts/*Drawable.cs`).

**Converters (`Converters/`):** `BoolToColorConverter`, `BoolToLayoutConverter`, `StringToBoolConverter` — chuyển giá trị binding sang thuộc tính UI trong XAML.

**Charts (`Views/Charts/`):** `OrderTrendChartDrawable`, `InventoryDonutDrawable` — implement `IDrawable`, vẽ biểu đồ bằng `Microsoft.Maui.Graphics` (không dùng thư viện chart ngoài).

---

## 8. State Management

Client **không dùng thư viện state ngoài** (không Redux/MVVM framework). State quản lý bằng:

- **`ApiClientProvider` (static)** — session toàn cục: `Token`, `Role`, `UserId`, `HttpClient` dùng chung (`ApiClientProvider.cs`). Đây là "single source of truth" cho phiên đăng nhập.
- **Property + `OnPropertyChanged`** trên từng `ContentPage` (mỗi page tự là BindingContext của chính nó) — cập nhật UI qua binding.
- **`App.SignalR` (static `SignalRService`)** — kênh realtime toàn cục; các page đăng ký/hủy event (`OnOrderStatusUpdated`, `OnPaymentResult`, `OnNewNotification`).
- **Shell switching** (`App.xaml.cs`) — chuyển trạng thái ứng dụng: `ShowLogin` / `ShowShell` (nội bộ) / `ShowCustomerShell` (khách).

**Đánh giá:** đủ dùng cho quy mô đồ án; state phân tán ở page nên khó tái sử dụng (xem [Điểm cần chú ý](#16-những-điểm-cần-chú-ý)).

---

## 9. Routing

**MAUI Shell** làm router. Hai shell:
- `AppShell` (nội bộ) — `AppShell.xaml.cs` đăng ký route: `RegisterPage`, `ProductDetailPage`, `ProductFormPage`, `OrderDetailPage`; `ApplyRoleVisibility()` ẩn/hiện flyout theo role (admin thấy tất cả; manager thấy dashboard/imports; staff mặc định Orders) (`AppShell.xaml.cs:16-29`).
- `CustomerShell` (khách) — `CustomerShell.xaml` khai báo 7 flyout (Trang chủ/Sản phẩm/Giỏ/Đơn/Địa chỉ/Hồ sơ/Thông báo); `CustomerShell.xaml.cs` đăng ký route chi tiết: `ProductDetailPage`, `OrderDetailPage`, `OrderTrackingPage`, `PaymentPage`.

**Điều hướng:** `Shell.Current.GoToAsync("//route")` (tab tuyệt đối) hoặc `GoToAsync("PageName?param=...")` (route đã đăng ký + query).

**"Protected route":** không có guard route riêng; bảo vệ nằm ở **backend** (mọi API `[Authorize]`). Client chỉ mở shell sau khi login thành công; `SessionGuard.HandleUnauthorizedAsync` đưa về LoginPage khi gặp 401.

---

## 10. API Layer (toàn bộ endpoint)

> Prefix `/api`. Policy trong ngoặc (mặc định `[Authorize]` nếu không ghi Anonymous).

**Auth** (`AuthController`): `POST /auth/register` (Anonymous) · `POST /auth/login` (Anonymous) · `GET /auth/profile` · `PUT /auth/profile` · `PUT /auth/change-password`

**Product** (`ProductController`): `GET /product` (ProductRead) · `GET /product/{id}` (ProductRead) · `GET /product/{id}/detail` (StaffOrAbove) · `GET /product/{id}/public-detail` (ProductRead) · `POST /product` (ProductWrite) · `PUT /product/{id}` (ProductWrite) · `POST /product/{id}/import` (InventoryWrite) · (+ delete/status — xem file)

**Category** (`CategoryController`): `GET /category` · `GET /category/{id}` · `POST /category` (ManagerOrAdmin) · `PUT /category/{id}` (ManagerOrAdmin) · `PUT /category/{id}/active` (ManagerOrAdmin) · `DELETE /category/{id}` (ManagerOrAdmin) · `GET /category/{id}/products` (ProductRead)

**Cart** (`CartController`, all Authorize): `GET /cart` · `POST /cart/items` · `PUT /cart/items/{maSanPham}?soLuong=` · `DELETE /cart/items/{maSanPham}` · `DELETE /cart`

**Address** (`AddressController`, all Authorize): `GET /address` · `POST /address` · `PUT /address/{id}` · `DELETE /address/{id}` · `PUT /address/{id}/default`

**Order** (`OrderController`): `POST /order` (OrderWrite) · `GET /order` · `GET /order/{id}` · `GET /order/full` (OrderManage) · `PUT /order/{id}/status` (OrderManage) · `PUT /order/{id}/cancel` · `GET /order/{id}/tracking`

**Payment** (`PaymentController`): `POST /payment/initiate` (OrderWrite) · `GET /payment/vnpay/return` (Anonymous) · `GET|POST /payment/vnpay/ipn` (Anonymous) · `GET /payment/momo/return` (Anonymous) · `POST /payment/momo/ipn` (Anonymous) · `GET /payment/status/{orderId}`

**Shipping** (`ShippingController`): `POST /shipping/estimate` · `POST /orders/{id}/shipping` (OrderManage) · `PUT /orders/{id}/shipping` (OrderManage) · `PUT /orders/{id}/shipping/status` (OrderManage) · `GET /orders/{id}/shipping` · `GET /shipping/tracking/{trackingNumber}` · `GET /orders/{id}/shipping/history`

**Notification** (`NotificationController`, all Authorize): `GET /notification` · `GET /notification/unread-count` · `PUT /notification/{id}/read` · `PUT /notification/read-all`

**Inventory** (`InventoryController`): `GET /inventory/products/{id}` (InventoryRead) · `GET /inventory/transactions` (InventoryRead) · `POST /inventory/adjustments` (InventoryWrite) · `POST /inventory/reconcile` (InventoryRead, fix→InventoryApprove)

**Warehouse Receipts** (`/warehouse-receipts`): `GET` · `GET /{id}` (InventoryRead) · `POST` · `PUT /{id}` · `PUT /{id}/submit` · `PUT /{id}/cancel` · `DELETE /{id}` (InventoryWrite) · `PUT /{id}/approve` · `PUT /{id}/complete` (InventoryApprove)

**Warehouse Issues** (`/warehouse-issues`): `GET` · `GET /history` · `GET /{id}` (InventoryRead) · `POST` · `PUT /{id}` · `PUT /{id}/submit` · `PUT /{id}/cancel` · `DELETE /{id}` (InventoryWrite) · `PUT /{id}/complete` (InventoryApprove)

**Report** (`/api/reports` hoặc `/api/report`): `GET /dashboard` · `/categories` · `/inventory` · `/shipping` · `/orders` · `/top-products` · `/logs` (ReportView) · `/revenue` · `/revenue-by-date` (RevenueView)

**Chat** (`/api/chat`, InternalOnly): `GET /users` · `GET|POST /conversations` · `POST /groups` · `PUT /conversations/{id}/name` · `GET /conversations/{id}/info|messages` · `POST /messages` · `POST /attachments` · `POST /messages/{id}/pin|recall|reaction` · `POST /calls/log` · `POST /polls` · `POST /messages/{id}/poll/vote|options|lock` · `POST /conversations/{id}/read`

**Admin** (`/api/admin`, AdminOnly): `GET /users` · `GET /users/{id}` · `GET /users/{id}/login-history` · `POST /users|create-user|create-manager|create-staff` · `PUT /users/{id}` · `PUT /users/{id}/active` · `PUT /users/{id}/password` · `DELETE /users/{id}`

**Audit** (`/api/audit-logs`, AuditRead): `GET` · `GET /{id}`
**System error** (`/api/system-error-logs`, SystemErrorLogRead): `GET` · `GET /{id}`
**Health** (`GET /health`, Anonymous).

**SignalR Hubs:** `/hubs/chat` (ChatHub) · `/hubs/order` (OrderHub) — JWT qua `?access_token=` (`Program.cs:504-505, 133-139`).

---

## 11. Database

**ORM:** EF Core (`AppDbContext`), Provider Npgsql. Schema tạo bằng `EnsureCreated()` + raw SQL bổ sung trong `Program.cs:211-392` (kèm 6 migration trong `Migrations/`).

**26 DbSet (bảng)** — nhóm theo miền:
- **Người dùng/quyền:** `NguoiDung` (MaNguoiDung, Email, TenDangNhap, MatKhauHash, MaVaiTro→PhanQuyen, IsActive, Ho, Ten, SoDienThoai), `PhanQuyen` (MaVaiTro, TenVaiTro).
- **Sản phẩm:** `SanPham` (MaSanPham, TenSanPham, GiaBan, GiaNhap, SoLuongTon, TonKhoBanDau, MucTonThap, TrangThai, HinhAnhUrl, MoTa, MaDanhMuc→DanhMuc), `DanhMucSanPham`.
- **Giỏ:** `GioHang` (MaGioHang, MaNguoiDung), `ChiTietGioHang` (MaGioHang→GioHang, MaSanPham, SoLuong).
- **Đơn:** `DonHang` (MaDonHang, MaNguoiDung, TongTien, TrangThai, NgayTao, ConcurrencyVersion, IdempotencyKey, TenNguoiNhan/SoDienThoai/DiaChiGiaoHang/TinhThanh/PhuongXa/LoaiDichVu/TrongLuongKg/GhiChu, PhuongThucThanhToan), `ChiTietDonHang` (MaDonHang→DonHang, MaSanPham→SanPham, SoLuong, DonGia), `ThanhToan`.
- **Địa chỉ/thông báo:** `DiaChi` (MaNguoiDung, TenNguoiNhan, SoDienThoai, TinhThanh/QuanHuyen/PhuongXa/DiaChiChiTiet, LaMacDinh), `ThongBao` (MaNguoiDung, LoaiThongBao, TieuDe, NoiDung, DaDoc, MaDonHang).
- **Kho:** `GiaoDichKho`, `PhieuNhapKho`+`ChiTietPhieuNhap`, `PhieuXuatKho`+`ChiTietPhieuXuat`.
- **Vận chuyển:** `VanChuyen` (MaVanChuyen, MaDonHang, DonViVanChuyen, MaVanDon, TrangThaiGiaoHang, PhiVanChuyen, NgayGiaoDuKien/ThucTe, ConcurrencyVersion), `LichSuVanChuyen`.
- **Chat:** `ChatConversation`, `ChatMessageEntity`, `ChatParticipant`, `ChatPoll`, `ChatPollOption`, `ChatPollVote`.
- **Nhật ký:** `AuditLog`, `SystemErrorLog`.

**Quan hệ chính (ERD rút gọn):**
```
NguoiDung 1─* DonHang 1─* ChiTietDonHang *─1 SanPham *─1 DanhMucSanPham
NguoiDung 1─1 GioHang 1─* ChiTietGioHang *─1 SanPham
NguoiDung 1─* DiaChi          NguoiDung 1─* ThongBao
DonHang  1─1 VanChuyen 1─* LichSuVanChuyen     DonHang 1─* ThanhToan
SanPham  1─* GiaoDichKho      PhieuNhapKho 1─* ChiTietPhieuNhap
NguoiDung *─1 PhanQuyen
```

**Index/ràng buộc tiêu biểu** (raw SQL trong `Program.cs`): unique `IdempotencyKey`, index `GioHang(MaNguoiDung)`, `ThongBao(MaNguoiDung, DaDoc)`, unique conversation 1-1, v.v. Precision tiền tệ cấu hình trong `AppDbContext` (`HasPrecision`).

---

## 12. Authentication

- **Cấp token:** `AuthService.Login` sinh JWT (claims `user_id`, `Name`, `Email`, `Role`), ký HMAC-SHA256, hạn 2h (`AuthService.cs:126-146`).
- **Xác thực token:** `Program.cs:118-143` cấu hình `JwtBearer` — `ValidateIssuer/Audience/Lifetime/IssuerSigningKey`. WebSocket (SignalR) lấy token từ `?access_token=` (`Program.cs:133-139`).
- **Phân quyền:** 15+ policy theo role (`Program.cs:145-191`): `AdminOnly`, `InternalOnly`, `StaffOrAbove`, `ManagerOrAdmin`, `ProductRead/Write`, `InventoryRead/Write/Approve`, `OrderWrite/Manage`, `AuditRead`, `SystemErrorLogRead`, `ReportView`, `RevenueView`.
- **Cô lập dữ liệu:** controller đọc claim `user_id`, mọi truy vấn khách lọc `MaNguoiDung == userId`; role nội bộ mới xem toàn bộ (ví dụ `OrderController.GetOrders:79-139`).
- **Không có refresh token / logout server-side:** token hết hạn thì client bắt 401 → `SessionGuard` đưa về login (`SessionGuard.cs`).

---

## 13. Security

| Khía cạnh | Hiện trạng trong source |
|---|---|
| **Password hash** | ✅ BCrypt (`BCrypt.Net.BCrypt.HashPassword/Verify`) — không lưu plaintext |
| **JWT** | ✅ Ký HMAC-SHA256, verify issuer/audience/lifetime |
| **Phân quyền** | ✅ Policy-based `[Authorize(Policy=...)]` + lọc theo `user_id` |
| **Input validation** | ✅ Model validation → `ApiErrorResponse` (`Program.cs:22-51`); validate nghiệp vụ trong service |
| **SQL Injection** | ✅ EF Core tham số hóa; raw SQL chỉ là DDL cố định (`Program.cs`) |
| **Xử lý lỗi tập trung** | ✅ `ApiExceptionHandler` + `UseExceptionHandler` → không lộ stack trace (`Program.cs:445-497`) |
| **CORS** | ⚠️ Không tìm thấy cấu hình CORS trong `Program.cs` (client là desktop nên ít ảnh hưởng) |
| **HTTPS** | ⚠️ Dev dùng http://localhost:5292; có profile https (`launchSettings.json`) nhưng client trỏ http |
| **Rate limit / CSRF** | ❌ Không tìm thấy trong source |
| **Secrets** | ⚠️ `Jwt:Key` và VnPay/MoMo mẫu để trong `appsettings.json` (nên chuyển sang `.env`/user-secrets cho production) |

---

## 14. Design Pattern

- **Layered / Service pattern:** Controller (nhận HTTP) → Service (business) → EF Core. Rõ ràng, tách trách nhiệm.
- **Dependency Injection:** toàn bộ service/DbContext/hub đăng ký DI (`Program.cs:54-207`), inject qua constructor.
- **Unit of Work + Repository (ẩn):** `AppDbContext` (EF Core) là UoW; DbSet là repository — không tự viết Repository riêng.
- **DTO pattern:** tách entity ↔ dữ liệu vào/ra (`DTOs/`).
- **Options/Config pattern:** đọc cấu hình JWT/VnPay/MoMo qua `IConfiguration`.
- **Observer (realtime):** SignalR Hub + client event handlers (`SignalRService`).
- **Idempotency + Optimistic Concurrency:** key idempotency + `ConcurrencyVersion` khi đổi trạng thái đơn (`OrderService`).
- **Hosted Service (background):** `ShippingAutoCompletionService` (`Program.cs:207`).
- **Middleware pipeline:** HTTP log, exception handler, status-code pages (`Program.cs:432-497`).

---

## 15. Thư viện sử dụng

| Thư viện | Dùng để làm gì | File tiêu biểu |
|---|---|---|
| EF Core + Npgsql | ORM PostgreSQL | `AppDbContext.cs`, mọi Service |
| JwtBearer + `System.IdentityModel.Tokens.Jwt` | Cấp/verify JWT | `AuthService.cs`, `Program.cs` |
| BCrypt.Net-Next | Hash mật khẩu | `AuthService.cs`, `AdminController.cs` |
| SignalR (server) | Realtime | `Hubs/*`, `NotificationService.cs` |
| SignalR.Client | Nhận realtime | `Services/SignalRService.cs` |
| Swashbuckle | Swagger UI | `Program.cs:77-109` |
| `System.Text.Json` | Serialize/deserialize | client Models + MoMo/VnPay service |
| `Microsoft.Maui.Graphics` | Vẽ biểu đồ | `Views/Charts/*` |
| HMACSHA256 (`System.Security.Cryptography`) | Ký callback VNPay/MoMo | `MoMoService.cs`, `VnPayService.cs` |

---

## 16. Những điểm cần chú ý

**Bug/điểm dễ nhầm (dựa trên source):**
1. **Checkout không gửi `Idempotency-Key`** (`CartPage.OnCheckoutClicked`) → double-tap nhanh có thể tạo 2 đơn (server có hỗ trợ idempotency nhưng client chưa dùng). *Đề xuất: sinh `Guid` mỗi lần checkout.*
2. **MoMo cần cấu hình thật để chạy:** credential mẫu trong `appsettings.json` bị từ chối; `RedirectUrl/IpnUrl=localhost` khiến IPN server-to-server không gọi về được (cần ngrok/domain). `appsettings.Development.json` đã đặt credential sandbox công khai của MoMo cho dev.
3. **Phí ship dễ bị hiểu nhầm là "giá ×3":** tổng đơn = subtotal + `deliveryDays×10k`; đơn nhỏ tới tỉnh xa, phí ship có thể ≈ 2× giá SP. Đã tách dòng hiển thị ở `PaymentPage` để minh bạch.
4. **`GiaNhap` (giá vốn) lộ qua `GET /api/product`** cho cả khách (DTO `ProductResponseDTO` chứa `GiaNhap`) — UI ẩn nhưng dữ liệu vẫn về client. *Đề xuất: DTO riêng cho khách.*
5. **Không refresh token / logout server-side:** token sống 2h; hết hạn phải đăng nhập lại.
6. **Không có CORS/rate-limit/HTTPS bắt buộc** (chấp nhận được cho đồ án desktop nội bộ).

**Code smell:**
- State client phân tán trên từng page (không tách ViewModel) → khó tái sử dụng/test.
- Một số controller (`ReportController`, `AdminController`) chứa logic truy vấn trực tiếp thay vì qua service.
- `DemoData.cs` (client) là dữ liệu mẫu **không còn được tham chiếu** (dead code) — nên xóa.

**Thiếu:** unit test (không tìm thấy project test trong source), retry/cache phía client, phân trang server-side cho danh sách sản phẩm.

---

## 17. Đánh giá chất lượng

| Tiêu chí | Điểm /10 | Lý do (dựa trên source) |
|---|---|---|
| **Architecture** | 8 | Phân tầng rõ (Controller/Service/EF), DI đầy đủ, DTO tách bạch, realtime SignalR, background service. Thiếu tầng test. |
| **Readability** | 7.5 | Tên biến tiếng Việt nhất quán, comment nhiều; một số comment bị lỗi encoding (mojibake) trong vài file service. |
| **Maintainability** | 7 | Backend dễ mở rộng theo service; client thiếu ViewModel nên logic dồn vào code-behind. |
| **Security** | 7 | BCrypt + JWT + policy + validation + xử lý lỗi tập trung tốt; thiếu CORS/rate-limit/secret management, secret mẫu trong appsettings. |
| **Scalability** | 6.5 | Idempotency + optimistic concurrency tốt cho tồn kho/đơn; nhưng lọc/phân trang sản phẩm ở client, chưa cache. |
| **Performance** | 7 | Dùng `AsNoTracking` cho truy vấn đọc, group tại DB; vài chỗ tải toàn bộ rồi xử lý ở app (report, product list). |

**Tổng quan:** Đây là đồ án **chất lượng khá tốt cho môn học**, thể hiện đúng trọng tâm mạng (nhiều client, realtime, concurrency). Backend chỉn chu hơn client.

---

## 18. Báo cáo tiến độ

### ✅ Đã hoàn thành
- Xác thực: đăng ký/đăng nhập/JWT/phân quyền theo role, hồ sơ, đổi mật khẩu.
- Khách: sản phẩm (lọc/phân trang/chi tiết), giỏ hàng, địa chỉ, đặt hàng (idempotency+concurrency), thanh toán COD/VNPay/MoMo, theo dõi đơn realtime, đơn của tôi, tự hủy đơn, thông báo.
- Nội bộ: quản lý sản phẩm/danh mục, kho (nhập/xuất/điều chỉnh/kiểm kê có duyệt), đơn hàng, vận chuyển, báo cáo/dashboard, chat nội bộ (poll/file/gọi), audit log, system error log, quản trị người dùng.
- Hạ tầng: SignalR (Order+Chat hub), health check, logging có cấu trúc, xử lý lỗi tập trung, seed DB + tài khoản admin.

### ⚠️ Còn thiếu / chưa hoàn chỉnh
- **Đánh giá sản phẩm (review):** endpoint chi tiết luôn trả `ReviewCount=0`, `AverageRating=0` (`ProductService.GetDetail:311-312`) → chưa có tính năng review thật.
- **Thanh toán online end-to-end thật:** cần credential sandbox thật + URL public (ngrok) cho IPN.
- **Client:** phân trang/lọc server-side, cache, ViewModel, refresh token.
- **Kiểm thử:** không tìm thấy project unit/integration test.

### 🐞 Bug còn tồn tại (xem mục 16)
- Checkout thiếu idempotency-key (double-tap → đơn trùng).
- `GiaNhap` lộ qua list API cho khách.
- MoMo phụ thuộc cấu hình môi trường (credential + ngrok).

### 💡 Đề xuất cải thiện
1. Client gửi `Idempotency-Key` khi checkout.
2. Tách DTO khách (ẩn `GiaNhap`/doanh thu) cho toàn bộ endpoint sản phẩm.
3. Thêm CORS/rate-limit; đưa secret (JWT/VnPay/MoMo) sang `.env`/user-secrets.
4. Bổ sung refresh token + logout server-side.
5. Tách ViewModel (MVVM) cho client để dễ test/tái sử dụng.
6. Thêm unit test cho `OrderService` (idempotency/concurrency) và verify chữ ký thanh toán.
7. Xóa `DemoData.cs` (dead code).

---

## 19. Hướng dẫn cài đặt & chạy dự án (từ số 0 đến hoàn thiện)

> Mục tiêu: một người **chưa từng chạm vào project** làm theo từ trên xuống là chạy được cả backend lẫn app MAUI. Mọi bước dựa trên script/cấu hình thật trong repo (`scripts/`, `run-*.bat`, `.env.example`, `Program.cs`).

### 19.0 Bức tranh tổng thể (cần nhớ trước)
Hệ thống gồm **2 tiến trình** phải chạy song song:
1. **Backend** `SyncChain.API` — cổng `http://localhost:5292` (REST + SignalR).
2. **Client** `app/SyncChain.Desktop` — app MAUI, gọi backend qua HTTP.

Và **1 dịch vụ ngoài**: **PostgreSQL** (local hoặc Neon cloud) — backend đọc `DATABASE_URL` từ `.env`.

Thứ tự khởi động **bắt buộc**: **Database sẵn sàng → Backend (chờ `/health` = 200) → Client**.

---

### 19.1 Cài môi trường (prerequisites)

| Thành phần | Bắt buộc | Ghi chú |
|---|---|---|
| **.NET 10 SDK** | ✅ | Bản đã dùng để build project: **10.0.301**. Kiểm tra: `dotnet --version` |
| **MAUI workload** | ✅ (cho client) | Cài: `dotnet workload install maui` (hoặc `maui-windows`). Kiểm tra: `dotnet workload list` |
| **PostgreSQL** | ✅ | Local (cài PostgreSQL 14+) **hoặc** tài khoản Neon cloud |
| **Windows 10/11** | ✅ | Client target `net10.0-windows10.0.19041.0` (chỉ chạy trên Windows) |
| **Git** | ✅ | Để clone repo |
| PowerShell 5.1+ | ✅ | Các script `.ps1` (Windows có sẵn) |
| Visual Studio 2022+ | ⛔ tùy chọn | Có thể mở `NT106_SyncChain.sln`; **không bắt buộc** — chỉ cần CLI `dotnet` |
| ngrok | ⛔ tùy chọn | Chỉ cần khi test **IPN** thanh toán online (MoMo/VNPay gọi callback server-to-server) |

**Kiểm tra nhanh sau khi cài:**
```powershell
dotnet --version          # -> 10.0.x
dotnet workload list      # -> phải thấy "maui" (hoặc maui-windows)
```
Nếu thiếu MAUI workload → chạy `dotnet workload install maui` (chạy PowerShell/CMD với quyền Admin).

---

### 19.2 Lấy source code
```powershell
git clone <URL_REPO> NT106_SyncChain
cd NT106_SyncChain
```

---

### 19.3 Cấu hình môi trường (`.env`)

Backend đọc chuỗi kết nối DB từ file **`.env`** ở thư mục gốc (qua `Configuration/EnvFileLoader.cs`), **không** dùng `appsettings.ConnectionStrings`.

**Bước 1 — tạo `.env` từ mẫu:**
```powershell
Copy-Item .env.example .env
```

**Bước 2 — sửa `DATABASE_URL` trong `.env`:**
- **Nếu dùng PostgreSQL LOCAL:**
  ```
  DATABASE_URL=postgresql://postgres:matkhau@localhost:5432/syncchain
  ```
  (đổi `matkhau` thành mật khẩu Postgres của bạn; `syncchain` là tên database).
- **Nếu dùng Neon cloud:** dán chuỗi kết nối Neon (dạng `postgresql://user:pass@host/db?sslmode=require`).

> Quy tắc đọc: script và backend đều lấy **dòng `DATABASE_URL` đầu tiên** không phải comment (`scripts/_common.ps1` → `Get-DatabaseEndpoint`; `EnvFileLoader`).

**Bước 3 (tùy chọn) — cấu hình thanh toán / JWT:** đã có sẵn giá trị trong `SyncChain.API/appsettings.json` (Jwt, VnPay, MoMo mẫu). Muốn test MoMo sandbox: đặt credential thật + URL public ở `appsettings.Development.json` hoặc biến môi trường `MoMo__*` (xem [mục 6.9](#69-thanh-toán-cod--vnpay--momo)).

---

### 19.4 Chuẩn bị Database

**Điểm quan trọng:** project **KHÔNG có bước migrate thủ công**. Khi backend khởi động, `Program.cs:211-430` tự:
- `db.Database.EnsureCreated()` — tạo toàn bộ **bảng** (schema).
- Chạy raw SQL bổ sung cột/bảng mới.
- **Seed**: 4 role (`customer/staff/manager/admin`) + tài khoản admin **`admin@gmail.com` / `123456`**.

→ Bạn **không cần** gõ lệnh migration nào cho môi trường dev.

**Nhưng lưu ý:** `EnsureCreated()` tạo *bảng*, **không tạo *database***.
- **Local:** phải tạo database rỗng trước (một lần):
  ```powershell
  # dùng psql (thay -U/-h nếu cần)
  createdb -U postgres syncchain
  # hoặc trong psql:  CREATE DATABASE syncchain;
  ```
- **Neon:** database đã tồn tại sẵn → bỏ qua bước này.

**Kiểm tra kết nối DB (không bắt buộc, chỉ để yên tâm):**
```powershell
powershell -ExecutionPolicy Bypass -File scripts\run-database.ps1
```
Script này đọc `.env`, test TCP tới host:port của DB, và nhắc rằng schema/seed sẽ tự tạo khi API chạy.

---

### 19.5 Chạy Backend — thủ công từng bước (hiểu bản chất)

```powershell
# 1) Khôi phục package
dotnet restore SyncChain.API

# 2) Build
dotnet build SyncChain.API -c Debug --nologo

# 3) Chạy
dotnet run --project SyncChain.API
```
Khi thành công, log sẽ hiện:
```
[Startup] Đang kết nối PostgreSQL và khởi tạo schema...
[Startup] Schema + seed hoàn tất (roles + tài khoản admin@gmail.com).
[Startup] SyncChain API sẵn sàng tại http://localhost:5292
```
**Kiểm chứng:**
- Health: mở `http://localhost:5292/health` → JSON `{ status: "healthy", database: "connected" }`.
- Swagger (thử API): `http://localhost:5292/swagger`.

> ⚠️ **Nếu build báo `MSB3027 ... file being used by another process`** nghĩa là **đang có 1 instance backend chạy** khóa file `.exe`. Dừng nó trước (`Stop-Process -Name SyncChain.API -Force`) rồi build lại.

---

### 19.6 Chạy Client MAUI — thủ công từng bước

```powershell
# 0) (một lần) đảm bảo có MAUI workload
dotnet workload install maui        # nếu chưa có

# 1) Khôi phục package
dotnet restore app/SyncChain.Desktop

# 2) Build (chỉ định TFF windows)
dotnet build app/SyncChain.Desktop -f net10.0-windows10.0.19041.0 -c Debug --nologo

# 3) Chạy
dotnet run --project app/SyncChain.Desktop -f net10.0-windows10.0.19041.0
```
- Màn hình **Login** sẽ tự probe backend qua `/health`; nếu backend chưa chạy → hiện thông báo "Máy chủ chưa sẵn sàng" (`LoginPage.OnAppearing`).
- Đăng nhập nội bộ: `admin@gmail.com` / `123456` (nút **Đăng nhập quản trị**). Đăng ký tài khoản khách để vào **cổng khách hàng**.

**Đổi địa chỉ backend (nếu không phải localhost):** đặt biến môi trường trước khi chạy:
```powershell
$env:SYNCCHAIN_API_URL = "http://192.168.1.10:5292/"
dotnet run --project app/SyncChain.Desktop -f net10.0-windows10.0.19041.0
```
(đọc ở `ApiClientProvider.cs:7-8`).

---

### 19.7 Chạy TOÀN BỘ bằng 1 lệnh (script có sẵn)

Repo đã có sẵn bộ script. **Cách nhanh nhất:**
```powershell
.\run-all.bat
```
`run-all.bat` chỉ là wrapper gọi `scripts/run-all.ps1` với `-ExecutionPolicy Bypass`. Logic `run-all.ps1` gồm **4 bước**:
1. Đọc `.env`, kiểm tra `DATABASE_URL` + test TCP tới DB (cảnh báo, không chặn).
2. **Mở cửa sổ mới** chạy `run-backend.ps1` (để log backend luôn hiển thị).
3. **Chờ** `GET /health` trả 200 (tối đa 120s) — `Wait-Backend`.
4. Chạy app Desktop trong cửa sổ hiện tại.

**Chạy riêng từng phần:**
```powershell
.\run-backend.bat     # chỉ backend  (scripts/run-backend.ps1: restore -> build -> run)
.\run-frontend.bat    # chỉ client   (scripts/run-frontend.ps1: cảnh báo nếu backend chưa lên, rồi restore/build/run)
```

**Cấu trúc bộ script (`scripts/`):**
| File | Vai trò |
|---|---|
| `_common.ps1` | Helper dùng chung: đường dẫn, `ApiPort=5292`, `Test-TcpPort`, `Get-DatabaseEndpoint`, `Wait-Backend` |
| `run-all.ps1` | Điều phối DB → backend (cửa sổ mới) → chờ health → desktop |
| `run-backend.ps1` | `dotnet restore/build/run` cho API |
| `run-frontend.ps1` | Kiểm tra backend rồi `dotnet restore/build/run` cho MAUI |
| `run-database.ps1` | Kiểm tra kết nối DB (không migrate — schema/seed tự động) |
| `run.ps1` (gốc repo) | Wrapper cũ, gọi `scripts/run-all.ps1` |

---

### 19.8 Tự viết file run từ số 0 (giải thích để bạn tự làm được)

Nếu muốn tự viết (hoặc giải thích với giảng viên), một script "chạy tất cả" tối giản cần đúng **4 ý**: (1) kiểm tra `.env`, (2) khởi động backend ở cửa sổ riêng, (3) **chờ tới khi `/health` = 200** (đây là mấu chốt — đừng chạy client khi backend chưa sẵn sàng), (4) chạy client.

**Ví dụ `my-run.ps1` tối giản (tự viết):**
```powershell
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$api  = Join-Path $root "SyncChain.API"
$app  = Join-Path $root "app/SyncChain.Desktop"
$health = "http://localhost:5292/health"
$tff  = "net10.0-windows10.0.19041.0"

# (1) .env phải tồn tại
if (-not (Test-Path "$root/.env")) { Write-Error "Thiếu .env (copy .env.example)"; exit 1 }

# (2) Backend ở cửa sổ mới để giữ log
Start-Process powershell -ArgumentList @(
  "-NoExit","-NoProfile","-ExecutionPolicy","Bypass",
  "-Command","dotnet run --project `"$api`""
)

# (3) Chờ /health = 200 (tối đa 120s)
$ok = $false
for ($i=0; $i -lt 60; $i++) {
  try { if ((Invoke-WebRequest $health -UseBasicParsing -TimeoutSec 5).StatusCode -eq 200) { $ok=$true; break } } catch {}
  Start-Sleep -Seconds 2; Write-Host "  ... chờ backend ($($i*2)s)"
}
if (-not $ok) { Write-Error "Backend không sẵn sàng"; exit 1 }

# (4) Client
dotnet run --project $app -f $tff
```

**Vì sao cần file `.bat` bọc ngoài `.ps1`?** Windows chặn chạy `.ps1` do Execution Policy. File `.bat` gọi `powershell -ExecutionPolicy Bypass -File ...` để **không phải chỉnh chính sách máy**. Đó chính là mẫu của `run-all.bat`:
```bat
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\run-all.ps1" %*
```
(`%~dp0` = thư mục chứa file .bat → đường dẫn tuyệt đối, chạy ở đâu cũng đúng.)

**Điểm mấu chốt khi tự viết:** dùng **`/health` làm cổng chờ** thay vì `Start-Sleep` cứng — vì thời gian backend khởi tạo schema/seed + kết nối DB cloud không cố định.

---

### 19.9 Kiểm thử (test scripts có sẵn)

Trong `scripts/` còn bộ test tự động (chạy sau khi backend đã lên):
- `test-core-e2e.mjs` — kịch bản E2E (Node): đăng ký → đăng nhập → đặt hàng → thanh toán...
- `test-order-conflicts.ps1`, `test-oversell.ps1` — kiểm thử **concurrency/idempotency** tồn kho & đơn.
- `test-shipping.ps1`, `test-reports.ps1`, `test-chat.ps1`, `test-audit-logs.ps1`, `test-system-error-logs.ps1`.

Ví dụ:
```powershell
node scripts/test-core-e2e.mjs
powershell -ExecutionPolicy Bypass -File scripts\test-oversell.ps1
```

> Không tìm thấy project unit-test (.csproj test) trong source — đây là các script kiểm thử qua HTTP, không phải xUnit/NUnit.

---

### 19.10 Xử lý sự cố thường gặp (troubleshooting)

| Triệu chứng | Nguyên nhân | Cách xử lý |
|---|---|---|
| `MSB3027 / MSB3021 ... file being used` khi build API | Backend đang chạy khóa `SyncChain.API.exe` | `Stop-Process -Name SyncChain.API -Force` rồi build lại |
| `dotnet build` MAUI báo thiếu workload | Chưa cài MAUI workload | `dotnet workload install maui` (quyền Admin) |
| Backend log "Chua cau hinh DATABASE_URL" | Thiếu `.env` hoặc sai `DATABASE_URL` | Tạo `.env` từ `.env.example`, sửa chuỗi kết nối |
| `/health` trả 503 / `database: disconnected` | DB chưa chạy / sai mật khẩu / chưa tạo database local | Bật PostgreSQL, `createdb syncchain`, kiểm tra `.env` |
| App MAUI báo "Máy chủ chưa sẵn sàng" ở màn Login | Backend chưa chạy hoặc chưa `/health`=200 | Chạy `run-backend.bat` trước, chờ log "sẵn sàng" |
| Port 5292 bị chiếm | Instance backend cũ còn chạy | Tắt tiến trình cũ hoặc đổi cổng trong `launchSettings.json` (nhớ đổi `SYNCCHAIN_API_URL` tương ứng) |
| Thanh toán MoMo/VNPay không nhận callback | `IpnUrl=localhost` không nhận server-to-server | Dùng ngrok expose cổng 5292, đặt lại `RedirectUrl/IpnUrl` public |
| `.ps1` không chạy được (execution policy) | Chính sách chặn script | Dùng file `.bat` bọc ngoài (đã có `run-*.bat`) hoặc `-ExecutionPolicy Bypass` |

---

### 19.11 Checklist "từ số 0 đến chạy được"
```
[ ] Cài .NET 10 SDK          → dotnet --version = 10.0.x
[ ] Cài MAUI workload        → dotnet workload install maui
[ ] Cài/kết nối PostgreSQL   → local: createdb syncchain | cloud: có chuỗi Neon
[ ] git clone repo
[ ] Copy .env.example -> .env, sửa DATABASE_URL
[ ] (kiểm tra) scripts\run-database.ps1  → DB reachable
[ ] Chạy backend             → run-backend.bat | thấy "API sẵn sàng" + /health OK
[ ] Chạy client              → run-frontend.bat | màn Login hiện ra
[ ] Đăng nhập admin@gmail.com / 123456  (hoặc đăng ký khách)
[ ] (hoặc) chạy 1 lệnh: run-all.bat
```

**Tóm tắt:** Chỉ cần .NET 10 SDK + MAUI workload + PostgreSQL. Sao chép `.env`, đặt `DATABASE_URL`, rồi **`run-all.bat`** dựng cả hệ thống (DB→backend→chờ `/health`→client). Không cần lệnh migration vì schema + seed (roles + `admin@gmail.com`/`123456`) tạo tự động khi API khởi động.

---

*Hết báo cáo. Mọi trích dẫn file/dòng dựa trên source tại thời điểm viết; nếu code thay đổi, cần đối chiếu lại.*
