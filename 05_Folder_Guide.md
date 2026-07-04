# Chương 5 — Phân tích toàn bộ thư mục

Mục tiêu chương này: sau khi đọc, bạn **mở repo lên là biết code nằm ở đâu**, hiểu **vì
sao mỗi thư mục tồn tại** (nó giải quyết vấn đề gì), và biết **khi nào mình cần đụng vào nó**.

Với mỗi thư mục ta trả lời 5 câu:

1. **Ví như gì** (một hình ảnh đời thực để nhớ).
2. **Vì sao cần** — vấn đề gì sẽ xảy ra nếu gộp chỗ này vào chỗ khác?
3. **Chứa gì** — liệt kê thật.
4. **Khi nào bạn cần mở nó ra sửa** — hướng dẫn thực dụng.
5. **Xóa đi thì sao** — để hiểu mức độ quan trọng.

---

## 5.1. Cây thư mục cấp cao

```
NT106_SyncChain/
├── SyncChain.API/            ★ Backend chính (ASP.NET Core .NET 10) — "nhà bếp"
├── app/SyncChain.Desktop/    ★ Client .NET MAUI (Windows) — "phòng ăn + thực đơn"
├── src/                      Bản prototype Node + Express (TÙY CHỌN, không nằm trong app chính)
├── ui/                       Giao diện web prototype (HTML/CSS/JS) (TÙY CHỌN)
├── database/TaoBang.sql      Backup schema PostgreSQL (tham khảo)
├── scripts/                  Script PowerShell chạy/kiểm thử dự án
├── docs/                     ← Sổ tay này
├── run-*.bat, run.ps1        Wrapper khởi chạy nhanh cho Windows
├── .env / .env.example       Cấu hình DATABASE_URL (.env bị gitignore — chứa bí mật)
├── README.md, README-DEV.md  Tài liệu tóm tắt & quy trình dev
└── NT106_SyncChain.sln       Solution mở bằng Visual Studio
```

**Một câu để nhớ toàn bộ:** hệ thống thật gồm **đúng hai phần chạy** — `SyncChain.API`
(server) và `app/SyncChain.Desktop` (client). Mọi thứ còn lại là **cấu hình, tài liệu,
tiện ích, hoặc bản prototype cũ**. Nếu bạn chỉ có thời gian đọc hai thư mục, hãy đọc hai
thư mục đó.

### Vì sao dự án chia thư mục kiểu này (đọc trước khi đi sâu)

Backend `SyncChain.API` không gom tất cả vào một chỗ mà tách thành **các tầng** (layer):
`Controllers → Services → Data/Models`. Lý do rất thực tế: mỗi tầng **chỉ lo một việc**, nên
khi có bug bạn biết ngay phải mở thư mục nào.

> **Ví dụ luồng một request** "khách bấm Đặt hàng":
>
> `OrderController` (nhận HTTP, kiểm quyền) → `OrderService` (luật: trừ kho, transaction,
> chống trùng) → `InventoryService` (đổi tồn an toàn) → `AppDbContext` (nói chuyện với DB) →
> bảng trong PostgreSQL. Kết quả trả ngược lại, đóng gói bằng một **DTO** rồi về client.
>
> Nếu lỗi "sai định dạng JSON trả về" → sửa **DTOs**. Lỗi "trừ kho sai" → sửa **Services**.
> Lỗi "gọi sai URL / thiếu quyền" → sửa **Controllers**. Lỗi "thiếu cột trong bảng" → sửa
> **Models + Data**. Việc chia thư mục chính là **tấm bản đồ chỉ chỗ sửa bug**.

---

## 5.2. `SyncChain.API/` — backend chính

**Ví như:** cái **nhà bếp** của nhà hàng. Khách không nhìn thấy nó, nhưng mọi món ăn (dữ
liệu) đều được nấu ở đây. Nếu xóa, **không còn hệ thống** — app MAUI mất chỗ để gọi, chỉ
còn là một cái vỏ rỗng.

Bên trong nhà bếp lại chia thành nhiều "khu" — mỗi thư mục con dưới đây là một khu:

### `Controllers/` — cửa vào HTTP
- **Ví như:** dãy **quầy tiếp tân**. Mỗi quầy nhận một loại yêu cầu (đơn hàng, sản phẩm,
  thanh toán...) rồi chuyển vào trong bếp.
- **Vì sao cần:** phải có một nơi **duy nhất** ánh xạ URL (`POST api/Order`) ↔ hàm C#. Đây
  cũng là nơi **kiểm quyền** (`[Authorize]`) và **đọc claim JWT** (user là ai, role gì).
- **Chứa gì:** `AuthController`, `ProductController`, `OrderController`, `CartController`,
  `PaymentController`, `ShippingController`, `InventoryController`,
  `WarehouseReceiptsController`, `WarehouseIssuesController`, `CategoryController`,
  `AddressController`, `NotificationController`, `ChatController`, `ReportController`,
  `AdminController`, `AuditLogsController`, `SystemErrorLogsController`.
- **Nguyên tắc vàng:** Controller **mỏng** — đọc request + kiểm quyền → gọi Service → trả
  kết quả. **Không** đặt nghiệp vụ phức tạp (transaction, tính tiền, trừ kho) ở đây. Nếu
  thấy Controller dài quá 1 màn hình, thường là logic đang bị đặt nhầm chỗ.
- **Khi nào bạn mở nó:** khi cần **thêm một endpoint mới**, đổi URL, hoặc đổi luật phân
  quyền của một API.
- **Xóa đi:** mất endpoint tương ứng → app không gọi được chức năng đó (bấm nút là lỗi 404).

### `Services/` — bộ não nghiệp vụ
- **Ví như:** các **đầu bếp chính**. Đây là nơi *thật sự* nấu: đặt luật, mở transaction,
  tính toán, đẩy realtime, gọi cổng thanh toán.
- **Vì sao cần:** nếu để logic trong Controller thì (1) không tái dùng được — ví dụ cả
  `OrderController` lẫn `PaymentController` đều cần "đẩy đơn sang processing"; (2) khó test;
  (3) Controller phình to. Tách ra Service giúp **giữ mỗi luật ở đúng một chỗ**.
- **Chứa gì:** `AuthService`, `OrderService`, `InventoryService`, `ProductService`,
  `CartService`, `ShippingService`, `ShippingAutoCompletionService` (chạy nền),
  `DeliveryEstimateService`, `WarehouseReceiptService`, `WarehouseIssueService`,
  `AddressService`, `ChatService`, `NotificationService`, `EmailService`, `VnPayService`,
  `MoMoService`, `AuditService`, `AuditContextAccessor`, `SystemErrorLogService`.
- **Khi nào bạn mở nó:** khi đổi **quy tắc nghiệp vụ** — cách trừ kho, điều kiện hủy đơn,
  cách tính phí ship, luật chuyển trạng thái... Đây là thư mục bạn sẽ ở lâu nhất.
- **Xóa đi:** Controller mất "bộ não" để gọi → chức năng vỡ ngay cả khi endpoint vẫn còn.

### `Data/` — cầu nối tới cơ sở dữ liệu
- **Ví như:** người **phiên dịch trưởng** giữa thế giới C# (object) và thế giới SQL (bảng).
- **Vì sao cần:** EF Core cần một nơi khai báo "hệ thống có những bảng nào, quan hệ ra sao".
- **Chứa:** `AppDbContext.cs` — khai báo toàn bộ `DbSet<>` (mỗi cái là một bảng) và cấu hình
  quan hệ, khóa, chỉ mục trong `OnModelCreating` (unique index chống trùng, concurrency
  token chống đua...).
- **Khi nào bạn mở nó:** khi **thêm một bảng mới** (thêm `DbSet<>`) hoặc chỉnh quan hệ /
  ràng buộc giữa các bảng.
- **Xóa đi:** EF Core không biết có bảng gì → không truy cập DB → **cả backend sập** ngay khi
  khởi động.

### `Models/` — định nghĩa "hình dạng" dữ liệu trong DB
- **Ví như:** **khuôn bánh**. Mỗi class Entity là khuôn cho một bảng; mỗi thuộc tính là một
  cột.
- **Vì sao cần:** phải mô tả bảng bằng class C# thì EF Core mới ánh xạ được. Tên đặt bằng
  **tiếng Việt không dấu** để khớp bảng trong DB (đây là quy ước của dự án).
- **Chứa gì:** Entity (`NguoiDung`, `SanPham`, `DonHang`, `ChiTietDonHang`, `GioHang`,
  `ChiTietGioHang`, `DiaChi`, `ThongBao`, `ThanhToan`, `VanChuyen`, `LichSuVanChuyen`,
  `PhieuNhapKho`, `ChiTietPhieuNhap`, `PhieuXuatKho`, `ChiTietPhieuXuat`, `GiaoDichKho`,
  `DanhMucSanPham`, `PhanQuyen`, `AuditLog`, `SystemErrorLog`, các `Chat*`); và các **lớp
  hằng số** gom "chuỗi ma thuật" về một chỗ: `OrderStatuses`, `ShippingStatuses`,
  `InventoryTransactionTypes`, `AuditActions`, `AuditResultStatuses`.
- **Khi nào bạn mở nó:** khi **thêm/bớt một cột** vào bảng, hoặc thêm một trạng thái mới.
- **Xóa đi:** mất định nghĩa bảng/hằng → không build được.

### `DTOs/` — hợp đồng dữ liệu với client
- **Ví như:** **tờ thực đơn**. Client chỉ được xem những gì ghi trên thực đơn, không được
  nhìn thẳng vào công thức trong bếp (Entity).
- **Vì sao cần — điểm quan trọng:** nếu trả thẳng Entity ra ngoài, bạn sẽ (1) **lộ dữ liệu
  nhạy cảm** (ví dụ `MatKhauHash` của `NguoiDung`); (2) bị **khóa cứng** — hễ đổi cột trong
  DB là vỡ giao diện client; (3) dễ dính lỗi vòng lặp JSON khi có quan hệ hai chiều. DTO là
  lớp trung gian **cố tình tách rời** hình dạng-gửi-đi khỏi hình dạng-lưu-trữ.
- **Chứa gì:** chia theo module thành thư mục con — `Address/`, `Admin/`, `Audit/`, `Cart/`,
  `Category/`, `Chat/`, `Inventory/`, `Payment/`, `Product/`, `Report/`, `Shipping/`,
  `SystemErrorLog/`, `WarehouseReceipt/`, `WarehouseIssue/`. Mỗi module thường có cặp
  `...Request` (client gửi lên) và `...Response`/`...Dto` (server trả về).
- **Khi nào bạn mở nó:** khi đổi **những gì API nhận vào hoặc trả ra** — thêm một trường vào
  request, ẩn bớt một trường trong response.
- **Xóa đi:** Controller không có kiểu để nhận/trả → không build.

### `Hubs/` — điểm cuối realtime (SignalR)
- **Ví như:** **loa phát thanh** của tòa nhà — đẩy tin xuống đúng người ngay lập tức, không
  cần client hỏi đi hỏi lại.
- **Vì sao cần:** HTTP thường là "hỏi mới có đáp". Muốn server **chủ động báo** ("đơn của bạn
  vừa chuyển sang đang giao", "có tin nhắn mới") thì cần kênh realtime — đó là SignalR.
- **Chứa:** `ChatHub.cs` (`/hubs/chat`), `OrderHub.cs` (`/hubs/order`).
- **Khi nào bạn mở nó:** khi thêm một loại sự kiện realtime mới, hoặc đổi cách chia nhóm
  người nhận.
- **Xóa đi:** mất realtime — thông báo đơn/thanh toán/chat không tự đẩy xuống client (app
  vẫn chạy nhưng phải bấm refresh mới thấy).

### `Configuration/` — nạp cấu hình môi trường
- **Chứa:** `EnvFileLoader.cs` — đọc file `.env` ở gốc repo và biến `DATABASE_URL` (dạng
  URL) thành chuỗi kết nối Npgsql mà .NET hiểu.
- **Vì sao cần:** .NET **không tự đọc `.env`**; và ta cố tình **không** để mật khẩu DB trong
  `appsettings.json` (vì file đó bị commit lên Git). Loader này là cầu nối để cả backend .NET
  lẫn prototype Node dùng **chung một `.env`** — bí mật nằm ngoài Git.
- **Xóa đi:** backend không đọc được cấu hình DB → không kết nối được PostgreSQL.

### `ExceptionHandling/` + `Exceptions/` — xử lý lỗi tập trung
- **Ví như:** **bộ phận chăm sóc khách hàng** — mọi sự cố đều được gói lại thành câu trả lời
  lịch sự, có mã, thay vì để lộ "ruột" hệ thống ra ngoài.
- **Vì sao cần:** nếu để exception thô rơi ra ngoài, client nhận về HTML lỗi khó đọc, lộ
  stack trace, và mỗi lỗi một kiểu. Ta muốn **mọi lỗi đều thành một JSON chuẩn**
  (`ApiErrorResponse` có `code`, `details`, `traceId`) để client xử lý đồng nhất.
- **Chứa:** `ApiExceptionHandler.cs` (biến exception → JSON chuẩn) và các exception nghiệp vụ
  trong `Exceptions/ApiException.cs` (`OutOfStockException`, `ConcurrencyConflictException`,
  `OrderNotFoundException`, `ValidationApiException`...).
- **Xóa đi:** lỗi trả về lộn xộn, mất mã lỗi thống nhất → client khó bắt lỗi đúng cách.

### `Migrations/` — lịch sử tiến hóa của schema
- **Chứa:** các migration EF Core (AddOrderAuditLog, AddShippingManagement, ExpandAuditLog,
  AddSystemErrorLog, AddOrderDeliveryInformation, AddInternalChat) + `AppDbContextModelSnapshot.cs`.
- **Vì sao cần (và lưu ý đặc thù dự án này):** thông thường migration là cách "nâng cấp bảng
  theo phiên bản". **Nhưng** dự án này khi khởi động dùng `EnsureCreated()` + các câu
  `ALTER TABLE ... IF NOT EXISTS` trong `Program.cs` để tự dựng schema, nên **không bắt buộc
  chạy migration tay**. Migrations ở đây mang tính **tài liệu hóa lịch sử** và để ai muốn
  dùng `dotnet ef database update` theo cách chuẩn.
- **Xóa đi:** *không* ảnh hưởng việc chạy, nhưng mất lịch sử tiến hóa schema.

### `wwwroot/` — file tĩnh phục vụ trực tiếp
- **Chứa:** file tĩnh trả thẳng qua HTTP, ví dụ ảnh/tệp đính kèm chat trong `uploads/`. Được
  bật bởi `app.UseStaticFiles()`.
- **Xóa đi:** không phục vụ được file tĩnh/đính kèm (ảnh chat không tải được).

### `Properties/launchSettings.json` — hồ sơ chạy khi dev
- **Chứa:** cấu hình chạy dev (URL, cổng **5292**, biến môi trường theo profile).
- **Xóa đi:** mất profile chạy mặc định (vẫn chạy được nếu chỉ định URL thủ công).

### File gốc nằm ngay trong `SyncChain.API/`
- **`Program.cs`** — **điểm khởi động, file quan trọng nhất backend**: nạp `.env`, cấu hình
  DI, JWT, Swagger, policy phân quyền, tự dựng schema + seed, dựng middleware pipeline, map
  hub, `/health`. Toàn bộ "bật công tắc" hệ thống nằm ở đây (phân tích kỹ ở chương 6).
- **`appsettings.json` / `appsettings.Development.json`** — cấu hình **JWT / Email / VnPay /
  MoMo** (giá trị sandbox). *Nhớ:* DB **không** đọc từ đây mà từ `.env`.
- **`SyncChain.API.csproj`** — khai target framework + danh sách NuGet package.
- **`SyncChain.API.http`** — script gọi thử API (REST client) để test tay không cần Postman.

---

## 5.3. `app/SyncChain.Desktop/` — client MAUI

**Ví như:** **phòng ăn kèm thực đơn** — nơi khách thật sự ngồi, xem, gọi món. Xóa thì mất
toàn bộ giao diện người dùng, **nhưng API vẫn sống** (vẫn gọi được bằng Swagger/Postman).

Vì sao là MAUI mà không phải web? Vì đây là bài tập **lập trình mạng**: mục tiêu là viết một
**ứng dụng desktop** tự gọi API qua HTTP và nhận realtime qua SignalR — đúng mô hình
client–server cổ điển.

### `Views/Pages/` — từng màn hình
- **Ví như:** từng **căn phòng** trong nhà. Mỗi màn hình là một cặp `*.xaml` (bố cục giao
  diện) + `*.xaml.cs` (xử lý sự kiện, gọi API).
- **Vì sao tách xaml / xaml.cs:** XAML lo *nhìn thế nào*, code-behind lo *làm gì khi bấm* —
  tách ra để dễ đọc và sửa độc lập.
- **Chứa:** `LoginPage`, `RegisterPage`, `CustomerHomePage`, `ProductsPage`,
  `ProductDetailPage`, `ProductFormPage`, `CartPage`, `CreateOrderPage`, `PaymentPage`,
  `CustomerOrdersPage`, `OrderTrackingPage`, `OrderDetailPage`, `OrdersPage`, `AddressPage`,
  `ProfilePage`, `NotificationPage`, `DashboardPage`, `ImportsPage`, `LogsPage`, `ChatPage`,
  `UserAccessPage`.
- **Khi nào bạn mở nó:** khi thêm/sửa một màn hình chức năng.
- **Xóa một file:** mất đúng màn hình đó.

### `Views/Charts/` — vẽ biểu đồ dashboard
- **Chứa:** `InventoryDonutDrawable.cs`, `OrderTrendChartDrawable.cs` — vẽ biểu đồ bằng
  `IDrawable` (Microsoft.Maui.Graphics), tức **tự vẽ bằng code** thay vì kéo thư viện chart
  ngoài (vì bài tập ưu tiên tự làm).

### `Services/` — hạ tầng dùng chung phía client
- **Ví như:** **hệ thống điện nước** của căn nhà — không phải phòng nào cả, nhưng mọi phòng
  đều xài.
- **Chứa:** `ApiClientProvider` (một `HttpClient` dùng chung + gắn token + kiểm `/health`),
  `SignalRService` (kết nối realtime), `SessionGuard` (xử lý 401 tập trung → về Login),
  `OrderStatusDisplay` (map trạng thái → chữ tiếng Việt + màu), `AppLog` (ghi log
  `[Desktop/...]`), `SigninBackground`, `DemoData`.
- **Khi nào bạn mở nó:** khi đổi địa chỉ server, cách giữ token, hay cách xử lý mất phiên.
- **Xóa `ApiClientProvider`/`SignalRService`:** app mất khả năng gọi API / nhận realtime.

### `Models/` — lớp đọc JSON trả về từ API
- **Chứa:** `AppModels.cs`, `CustomerApiModels.cs` — các class C# để **giải mã JSON** mà API
  trả về (ví dụ `LoginResponseApi`).
- **Vì sao có "Model" cả hai bên:** đây là **DTO phía client**, khác với DTO phía server. Hai
  bên tự mô tả cùng một dữ liệu theo góc nhìn của mình — client chỉ cần các trường nó dùng.

### `Converters/` — chuyển đổi dữ liệu khi binding
- **Chứa:** `BoolToColorConverter`, `BoolToLayoutConverter`, `StringToBoolConverter`.
- **Vì sao cần:** XAML binding cần biến dữ liệu thô thành thứ hiển thị được — ví dụ `true` →
  màu xanh, `false` → màu xám. Converter làm cầu nối đó, tránh nhét logic hiển thị vào code.

### `Resources/` — tài nguyên giao diện
- **Chứa:** `Styles/Colors.xaml`, `Styles/Styles.xaml`, `Fonts/`, `Images/`, `AppIcon/`,
  `Splash/`, `Raw/`. Định hình **bộ nhận diện** (màu, font, icon, ảnh nền, màn hình chờ).
- **Khi nào bạn mở nó:** khi đổi theme màu, font, icon ứng dụng.

### `Platforms/` — mã đặc thù từng nền tảng
- **Chứa:** thư mục con cho `Windows` (dự án này dùng chính), và khung sẵn cho Android/iOS/
  MacCatalyst/Tizen của MAUI.
- **Vì sao cần:** MAUI là "một code chạy nhiều nền", nhưng vài thứ (đăng ký app, quyền, entry
  point native) phải viết riêng cho từng OS — chúng nằm ở đây.
- **Xóa `Platforms/Windows`:** không build được app Windows.

### File gốc trong `app/SyncChain.Desktop/`
- **`App.xaml(.cs)`** — điểm vào app; giữ một `SignalRService` **tĩnh**; các hàm `ShowShell`,
  `ShowCustomerShell`, `ShowLogin` để chuyển khung theo vai trò.
- **`AppShell.xaml(.cs)`** — khung điều hướng (menu) cho **nhân sự nội bộ**.
- **`CustomerShell.xaml(.cs)`** — khung điều hướng cho **khách hàng**. *Vì sao hai shell:* để
  khách và nhân sự thấy **menu hoàn toàn khác nhau** — khách không thấy được trang quản trị.
- **`MauiProgram.cs`** — cấu hình app MAUI (font, `HttpClient` chung, tinh chỉnh control trên
  Windows).
- **`GlobalXmlns.cs`** — khai báo namespace XAML dùng chung để bớt lặp trong từng file `.xaml`.
- **`SyncChain.Desktop.csproj`** — target `net10.0-windows...`, package MAUI + SignalR client.

---

## 5.4. `src/` + `ui/` — bản prototype (TÙY CHỌN)

**Ví như:** **bản nháp cũ** vẽ trên giấy trước khi xây nhà thật. Giữ lại để đối chiếu ý
tưởng, nhưng **không ai ở trong đó**.

- `src/` — Node + Express: `server.js` (API của prototype), `index.js` (script thử logic DB,
  trong đó có cách **đặt hàng an toàn bằng transaction** — cùng ý tưởng chống oversell với
  bản .NET nhưng viết SQL tay), `db.js` (pool kết nối `pg`), `seedDatabase.js`,
  `constants/statusEnum.js`, `middleware/validateStatusTransition.js`.
- `ui/` — trang HTML/CSS/JS thuần theo module (`login/`, `register/`, `dashboard/`,
  `product/`, `order/`, `imports/`, `chat/`, `checklogs/`, `users-modal.html`).
- **Vì sao vẫn để trong repo:** vừa là **lịch sử** (bản đầu tiên chạy được), vừa là **tài
  liệu đối chiếu** logic. Bản .NET kế thừa các quyết định thiết kế (enum trạng thái, chống
  oversell) từ đây.
- **Xóa cả hai:** **không ảnh hưởng** ứng dụng chính (MAUI + .NET API). Chỉ mất bản demo web.

> ⚠️ **Cạm bẫy thường gặp:** đừng nhầm `src/server.js` (Node, cổng 3000) là backend chính.
> Backend chính là `SyncChain.API` (.NET, cổng **5292**). App MAUI **chỉ** gọi cổng 5292.

---

## 5.5. `scripts/` + wrapper `.bat`/`.ps1` — tự động hóa

**Ví như:** **bảng công tắc tổng** — bấm một nút chạy được cả dây chuyền, thay vì gõ tay
từng lệnh.

- `scripts/_common.ps1` — hàm dùng chung (đọc `DATABASE_URL`, chờ `/health` sẵn sàng, in màu
  ra console).
- `scripts/run-database.ps1`, `run-backend.ps1`, `run-frontend.ps1`, `run-all.ps1` — chạy
  từng phần hoặc toàn bộ hệ thống.
- `scripts/test-*.ps1` và `test-core-e2e.mjs` — script **kiểm thử tự động** (audit, chat,
  oversell, order-conflicts, reports, shipping, system-error-logs, và e2e xuyên suốt).
- `run-all.bat`, `run-backend.bat`, `run-frontend.bat`, `run.ps1` (gốc repo) — wrapper gọi
  nhanh cho người ngại mở PowerShell.
- **Vì sao cần:** dựng đủ DB + backend + frontend theo đúng thứ tự khá lằng nhằng; script gói
  lại để **một lệnh là chạy**, và để test có thể lặp lại (không phụ thuộc thao tác tay).
- **Xóa:** vẫn chạy tay bằng `dotnet run` được; chỉ mất tiện lợi.

## 5.6. `database/TaoBang.sql`

- **Chứa:** script SQL tạo bảng.
- **Vì sao có:** chủ yếu phục vụ **prototype** `src/index.js` (Node tạo bảng bằng SQL tay) và
  làm tài liệu tham khảo schema.
- **Với backend .NET:** *không bắt buộc*, vì `EnsureCreated()` tự dựng schema từ Model.

## 5.7. Các file cấu hình / tài liệu ở gốc repo

- **`.env`** — chứa `DATABASE_URL` thật; **bị gitignore** vì có mật khẩu. Đây là file **bắt
  buộc phải tự tạo** khi setup máy mới.
- **`.env.example`** — bản mẫu (không có bí mật) để copy thành `.env`.
- **`README.md` / `README-DEV.md`** — tóm tắt dự án & quy trình dev.
- **`NT106_SyncChain.sln`** — file solution; mở bằng Visual Studio để thấy cả backend lẫn
  client trong một cửa sổ.

## 5.8. Thư mục sinh ra tự động (đừng bận tâm khi đọc code)

`bin/`, `obj/`, `.vs/` — sản phẩm build và cache của IDE, **bị gitignore**. Xóa an toàn;
build lại sẽ tạo lại. Ngoài ra còn vài thư mục `.build-check/`, `.dotnet-home*/` là
**artifact kiểm tra build tự động** — không phải mã nguồn, bỏ qua khi đọc code.

> **Mẹo định vị nhanh khi lạc:** cần đổi *luật* → `Services/`; cần đổi *URL/quyền* →
> `Controllers/`; cần đổi *hình dạng dữ liệu vào–ra* → `DTOs/`; cần đổi *cột/bảng* →
> `Models/` + `Data/`; cần đổi *giao diện* → `app/.../Views/Pages/`.

---

➡️ Tiếp theo: [06_File_Guide.md](06_File_Guide.md) — đi sâu từng file quan trọng: class,
method, phụ thuộc, luồng dữ liệu, trích đoạn code.
