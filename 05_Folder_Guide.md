# Chương 5 — Phân tích toàn bộ thư mục

Với mỗi thư mục: **vì sao cần, chứa gì, xóa đi thì ảnh hưởng gì.**

## 5.1. Cây thư mục cấp cao

```
NT106_SyncChain/
├── SyncChain.API/            ★ Backend chính (ASP.NET Core .NET 10)
├── app/SyncChain.Desktop/    ★ Client .NET MAUI (Windows)
├── src/                      Bản prototype Node + Express (TÙY CHỌN)
├── ui/                       Giao diện web prototype (HTML/CSS/JS) (TÙY CHỌN)
├── database/TaoBang.sql      Backup schema PostgreSQL (tham khảo)
├── scripts/                  Script PowerShell chạy/kiểm thử dự án
├── docs/                     ← Sổ tay này
├── run-*.bat, run.ps1        Wrapper khởi chạy nhanh cho Windows
├── .env / .env.example       Cấu hình DATABASE_URL (.env bị gitignore)
├── README.md, README-DEV.md  Tài liệu tóm tắt & quy trình dev
└── NT106_SyncChain.sln       Solution mở bằng Visual Studio
```

---

## 5.2. `SyncChain.API/` — backend chính

Đây là "nhà bếp". Nếu xóa, **không còn hệ thống** — app MAUI mất chỗ để gọi.

### `SyncChain.API/Controllers/`
- **Vì sao cần:** là **cửa vào HTTP**. Mỗi file là một nhóm endpoint REST.
- **Chứa gì:** `AuthController`, `ProductController`, `OrderController`, `CartController`,
  `PaymentController`, `ShippingController`, `InventoryController`,
  `WarehouseReceiptsController`, `WarehouseIssuesController`, `CategoryController`,
  `AddressController`, `NotificationController`, `ChatController`, `ReportController`,
  `AdminController`, `AuditLogsController`, `SystemErrorLogsController`.
- **Nhiệm vụ mỗi file:** đọc request + claim JWT → kiểm quyền → gọi Service → trả kết quả.
  **Không** chứa nghiệp vụ phức tạp.
- **Xóa đi:** mất endpoint tương ứng → app không gọi được chức năng đó.

### `SyncChain.API/Services/`
- **Vì sao cần:** chứa **nghiệp vụ** — nơi đặt luật, transaction, tính toán, đẩy realtime.
- **Chứa gì:** `AuthService`, `OrderService`, `InventoryService`, `ProductService`,
  `CartService`, `ShippingService`, `ShippingAutoCompletionService` (chạy nền),
  `DeliveryEstimateService`, `WarehouseReceiptService`, `WarehouseIssueService`,
  `AddressService`, `ChatService`, `NotificationService`, `EmailService`, `VnPayService`,
  `MoMoService`, `AuditService`, `AuditContextAccessor`, `SystemErrorLogService`.
- **Xóa đi:** Controller mất "bộ não" để gọi → chức năng vỡ.

### `SyncChain.API/Data/`
- **Chứa:** `AppDbContext.cs` — khai báo toàn bộ `DbSet<>` (bảng) và cấu hình quan hệ
  (`OnModelCreating`).
- **Xóa đi:** EF Core không biết có bảng gì → không truy cập DB được → cả backend sập.

### `SyncChain.API/Models/`
- **Vì sao cần:** các **Entity** ánh xạ với bảng DB + vài lớp hằng số.
- **Chứa gì:** Entity đặt tên tiếng Việt (`NguoiDung`, `SanPham`, `DonHang`,
  `ChiTietDonHang`, `GioHang`, `ChiTietGioHang`, `DiaChi`, `ThongBao`, `ThanhToan`,
  `VanChuyen`, `LichSuVanChuyen`, `PhieuNhapKho`, `ChiTietPhieuNhap`, `PhieuXuatKho`,
  `ChiTietPhieuXuat`, `GiaoDichKho`, `DanhMucSanPham`, `PhanQuyen`, `AuditLog`,
  `SystemErrorLog`, các `Chat*`); và các lớp hằng: `OrderStatuses`, `ShippingStatuses`,
  `InventoryTransactionTypes`, `AuditActions`, `AuditResultStatuses`.
- **Xóa đi:** mất định nghĩa bảng/hằng → không build được.

### `SyncChain.API/DTOs/`
- **Vì sao cần:** **hợp đồng request/response** của API, tách khỏi Entity. Chia theo module
  (thư mục con `Address/`, `Cart/`, `Product/`, `Order*`, `Payment/`, `Shipping/`, `Chat/`,
  `Report/`, `Inventory/`, `Audit/`, `WarehouseReceipt/`, `WarehouseIssue/`,
  `SystemErrorLog/`...).
- **Xóa đi:** Controller không có kiểu dữ liệu để nhận/trả → không build.

### `SyncChain.API/Hubs/`
- **Chứa:** `ChatHub.cs` (`/hubs/chat`), `OrderHub.cs` (`/hubs/order`) — điểm cuối SignalR
  để đẩy realtime.
- **Xóa đi:** mất realtime (thông báo đơn/thanh toán/chat không đẩy được xuống client).

### `SyncChain.API/Configuration/`
- **Chứa:** `EnvFileLoader.cs` — nạp `.env` và biến `DATABASE_URL` thành chuỗi kết nối Npgsql.
- **Xóa đi:** backend không đọc được cấu hình DB từ `.env`.

### `SyncChain.API/ExceptionHandling/` + `Exceptions/`
- **Chứa:** `ApiExceptionHandler.cs` (biến exception → JSON chuẩn) và các exception nghiệp
  vụ trong `Exceptions/ApiException.cs` (VD `OutOfStockException`, `ConcurrencyConflictException`,
  `OrderNotFoundException`, `ValidationApiException`...).
- **Xóa đi:** lỗi trả về lộn xộn, mất mã lỗi thống nhất.

### `SyncChain.API/Migrations/`
- **Chứa:** các migration EF Core đã tạo (AddOrderAuditLog, AddShippingManagement,
  ExpandAuditLog, AddSystemErrorLog, AddOrderDeliveryInformation, AddInternalChat) +
  `AppDbContextModelSnapshot.cs`.
- **Xóa đi:** không ảnh hưởng *chạy* (vì dùng `EnsureCreated()`), nhưng mất lịch sử tiến hóa
  schema và khả năng `dotnet ef database update`.

### `SyncChain.API/wwwroot/`
- **Chứa:** file tĩnh phục vụ trực tiếp (VD file upload chat trong `uploads/chat/`). Bật bởi
  `app.UseStaticFiles()`.
- **Xóa đi:** không phục vụ được file tĩnh/đính kèm.

### `SyncChain.API/Properties/launchSettings.json`
- **Chứa:** cấu hình chạy dev (URL, cổng `5292`, biến môi trường profile).
- **Xóa đi:** mất profile chạy mặc định (vẫn chạy được nếu chỉ định URL thủ công).

### File gốc trong `SyncChain.API/`
- `Program.cs` — **điểm khởi động**: DI, JWT, Swagger, policy, seed, middleware, map hub,
  `/health`. Đây là file quan trọng nhất backend.
- `appsettings.json` / `appsettings.Development.json` — cấu hình **JWT / Email / VnPay /
  MoMo** (giá trị sandbox). *Lưu ý:* DB **không** đọc từ đây mà từ `.env`.
- `SyncChain.API.csproj` — khai target framework + các NuGet package.
- `SyncChain.API.http` — script gọi thử API (dạng REST client).

---

## 5.3. `app/SyncChain.Desktop/` — client MAUI

Đây là "phòng ăn". Xóa thì mất giao diện người dùng (nhưng API vẫn sống).

### `Views/Pages/`
- **Chứa:** từng màn hình dạng cặp `*.xaml` (giao diện) + `*.xaml.cs` (xử lý). VD:
  `LoginPage`, `RegisterPage`, `CustomerHomePage`, `ProductsPage`, `ProductDetailPage`,
  `ProductFormPage`, `CartPage`, `CreateOrderPage`, `PaymentPage`, `CustomerOrdersPage`,
  `OrderTrackingPage`, `OrderDetailPage`, `OrdersPage`, `AddressPage`, `ProfilePage`,
  `NotificationPage`, `DashboardPage`, `ImportsPage`, `LogsPage`, `ChatPage`, `UserAccessPage`.
- **Xóa một file:** mất màn hình đó.

### `Views/Charts/`
- **Chứa:** `InventoryDonutDrawable.cs`, `OrderTrendChartDrawable.cs` — vẽ biểu đồ dashboard
  bằng `IDrawable` (Microsoft.Maui.Graphics).

### `Services/`
- **Chứa:** `ApiClientProvider` (HttpClient + token + `/health`), `SignalRService` (realtime),
  `SessionGuard` (xử lý 401 tập trung → về Login), `OrderStatusDisplay` (map trạng thái →
  chữ tiếng Việt/màu), `AppLog` (log `[Desktop/...]`), `SigninBackground`, `DemoData`.
- **Xóa `ApiClientProvider`/`SignalRService`:** app mất khả năng gọi API/realtime.

### `Models/`
- **Chứa:** `AppModels.cs`, `CustomerApiModels.cs` — các lớp C# để **đọc JSON** trả về từ API
  (VD `LoginResponseApi`). Đây là DTO **phía client**, khác DTO phía server.

### `Converters/`
- **Chứa:** `BoolToColorConverter`, `BoolToLayoutConverter`, `StringToBoolConverter` — bộ
  chuyển đổi dùng trong data binding XAML.

### `Resources/`
- **Chứa:** `Styles/Colors.xaml`, `Styles/Styles.xaml`, font, icon, splash, ảnh nền. Định
  hình giao diện.

### `Platforms/`
- **Chứa:** mã đặc thù từng nền tảng (`Windows`, `Android`, `iOS`, `MacCatalyst`, `Tizen`).
  Trên đồ án chủ yếu dùng **Windows**.
- **Xóa `Platforms/Windows`:** không build được app Windows.

### File gốc trong `app/SyncChain.Desktop/`
- `App.xaml` / `App.xaml.cs` — điểm vào app; giữ `SignalR` tĩnh; các hàm `ShowShell`,
  `ShowCustomerShell`, `ShowLogin`.
- `AppShell.xaml(.cs)` — khung điều hướng cho **nhân sự nội bộ**.
- `CustomerShell.xaml(.cs)` — khung điều hướng cho **khách hàng**.
- `MauiProgram.cs` — cấu hình app MAUI (font, HttpClient chung, tinh chỉnh control Windows).
- `GlobalXmlns.cs` — khai báo namespace XAML dùng chung.
- `SyncChain.Desktop.csproj` — target `net10.0-windows...`, package MAUI + SignalR client.

---

## 5.4. `src/` + `ui/` — bản prototype (TÙY CHỌN)

- `src/` — Node + Express: `server.js` (API thật của prototype), `index.js` (script thử
  logic DB), `db.js` (pool kết nối `pg`), `seedDatabase.js`, `constants/statusEnum.js`,
  `middleware/validateStatusTransition.js`.
- `ui/` — trang HTML/CSS/JS thuần theo module (`login/`, `register/`, `dashboard/`,
  `product/`, `order/`, `imports/`, `chat/`, `checklogs/`, `users-modal.html/`).
- **Xóa cả hai:** **không ảnh hưởng** ứng dụng chính (MAUI + .NET API). Chỉ mất bản demo web.

## 5.5. `scripts/` + wrapper `.bat`/`.ps1`

- `scripts/_common.ps1` — hàm dùng chung (đọc `DATABASE_URL`, chờ `/health`, in màu).
- `scripts/run-database.ps1`, `run-backend.ps1`, `run-frontend.ps1`, `run-all.ps1` — chạy
  từng phần / toàn bộ.
- `scripts/test-*.ps1` và `test-core-e2e.mjs` — script kiểm thử (audit, chat, oversell,
  order-conflicts, reports, shipping, system-error-logs, e2e).
- `run-all.bat`, `run-backend.bat`, `run-frontend.bat`, `run.ps1` — wrapper gọi nhanh.
- **Xóa:** vẫn chạy tay bằng `dotnet run` được; chỉ mất tiện lợi.

## 5.6. `database/TaoBang.sql`

- **Chứa:** script SQL tạo bảng (tham khảo / dùng bởi prototype `src/index.js`).
- **Với backend .NET:** *không bắt buộc*, vì `EnsureCreated()` tự dựng schema từ model.

## 5.7. Thư mục sinh ra tự động (đừng bận tâm, thường bị gitignore)

`bin/`, `obj/`, `.vs/` — sản phẩm build/cache của IDE. Có thể xóa an toàn; build lại sẽ tạo
lại. (Trong repo còn một số thư mục `.build-check/`, `.dotnet-home*/` là **artifact kiểm
tra build** — không phải mã nguồn, có thể bỏ qua khi đọc code.)

---

➡️ Tiếp theo: [06_File_Guide.md](06_File_Guide.md) — đi sâu từng file quan trọng: class,
method, phụ thuộc, luồng dữ liệu, trích đoạn code.
