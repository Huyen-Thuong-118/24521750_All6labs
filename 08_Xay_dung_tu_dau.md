# Chương 8 — Hướng dẫn xây dựng dự án từ đầu

Chương này **không dựa vào source có sẵn** mà hướng dẫn *tư duy dựng lại* SyncChain theo
trình tự. Mỗi bước trả lời câu hỏi **"tại sao làm bước này, và tại sao làm vào lúc này?"**.
Thứ tự này cũng chính là thứ tự nên làm khi bắt đầu một dự án client–server tương tự.

## Nguyên tắc thứ tự: "hạ tầng → danh tính → dữ liệu lõi → nghiệp vụ → realtime → client"

Ta luôn làm phần **bên dưới** trước, vì phần trên phụ thuộc phần dưới. Không thể làm "đặt
hàng" khi chưa có "sản phẩm"; không thể phân quyền khi chưa có "đăng nhập".

---

### Bước 1 — Tạo Solution + dự án Web API
- **Làm gì:** `dotnet new sln -n NT106_SyncChain`; `dotnet new webapi -n SyncChain.API`;
  thêm vào solution.
- **Tại sao:** Solution là "cặp hồ sơ" gom nhiều dự án (API + MAUI) để mở chung trong IDE.
  Web API là **bộ khung backend** — mọi thứ khác gắn vào đây.

### Bước 2 — Cài EF Core + Npgsql, tạo `AppDbContext`
- **Làm gì:** thêm package `Npgsql.EntityFrameworkCore.PostgreSQL`, tạo `AppDbContext : DbContext`.
- **Tại sao trước:** vì **mọi tính năng đều cần lưu dữ liệu**. Có DbContext rồi mới khai được
  các bảng. Đây là "đường ống" xuống database.

### Bước 3 — Cấu hình chuỗi kết nối qua `.env` (`EnvFileLoader`)
- **Làm gì:** viết `EnvFileLoader` đọc `DATABASE_URL`, gọi `EnvFileLoader.Load()` đầu
  `Program.cs`, rồi `AddDbContext(UseNpgsql(...))`.
- **Tại sao:** **không commit bí mật** vào mã. Tách config DB ra `.env` (gitignore) để mỗi
  người tự cấu hình, và để dùng chung với các thành phần khác (prototype Node).

### Bước 4 — Tạo Database (rỗng) + để backend tự dựng schema
- **Làm gì:** với local: `CREATE DATABASE syncchain;`. Trong `Program.cs`: `EnsureCreated()`.
- **Tại sao:** cần một DB tồn tại để nối; `EnsureCreated()` giúp **chạy phát là có bảng**,
  giảm rào cản cho người mới (không phải học migration ngay). (Về lâu dài, dự án lớn nên
  chuyển hẳn sang migration — xem [10_Tu_duy_thiet_ke.md](10_Tu_duy_thiet_ke.md).)

### Bước 5 — Thiết kế bảng theo đúng thứ tự phụ thuộc
- **Làm gì:** tạo các Entity theo lớp:
  1. **Nền tảng danh tính:** `PhanQuyen` (role) → `NguoiDung` (user tham chiếu role).
  2. **Danh mục & sản phẩm:** `DanhMucSanPham` → `SanPham`.
  3. **Kho:** `GiaoDichKho` (sổ cái), `PhieuNhapKho`/`ChiTietPhieuNhap`,
     `PhieuXuatKho`/`ChiTietPhieuXuat`.
  4. **Mua sắm:** `GioHang`/`ChiTietGioHang`, `DiaChi`.
  5. **Đơn hàng:** `DonHang` → `ChiTietDonHang` → `ThanhToan` → `VanChuyen`/`LichSuVanChuyen`.
  6. **Phụ trợ:** `ThongBao`, `AuditLog`, `SystemErrorLog`, các `Chat*`.
- **Tại sao thứ tự này:** **khóa ngoại phải trỏ tới bảng đã tồn tại**. `NguoiDung` cần
  `PhanQuyen` trước; `DonHang` cần `NguoiDung` + `SanPham` trước; `ChiTietDonHang` cần
  `DonHang` + `SanPham`; `ThanhToan`/`VanChuyen` cần `DonHang`. Làm ngược lại sẽ vướng ràng
  buộc. (EF `OnModelCreating` khai các quan hệ này.)

### Bước 6 — Seed dữ liệu tối thiểu (roles + admin)
- **Làm gì:** trong `Program.cs`, thêm 4 role và một tài khoản `admin` (mật khẩu băm BCrypt)
  nếu chưa có.
- **Tại sao:** phải có **ít nhất một tài khoản để đăng nhập** và các role để phân quyền —
  nếu không, hệ thống "khóa cứng" (không ai vào được để tạo user đầu tiên).

### Bước 7 — Xác thực: Đăng ký / Đăng nhập + JWT (LÀM TRƯỚC MỌI NGHIỆP VỤ)
- **Làm gì:** `AuthService` (băm/kiểm mật khẩu, tạo JWT), `AuthController`
  (`register`/`login`/`profile`). Cấu hình `AddJwtBearer` + `UseAuthentication/Authorization`.
- **Tại sao làm trước:** hầu hết endpoint sau này đều `[Authorize]`. Không có JWT thì không
  test được gì cần đăng nhập. Danh tính là "chìa khóa" mở mọi cửa còn lại.

### Bước 8 — Định nghĩa policy phân quyền theo role
- **Làm gì:** trong `Program.cs`, `AddAuthorization` với các policy (`OrderManage`,
  `ProductWrite`...). Gắn `[Authorize(Policy=...)]` lên endpoint.
- **Tại sao ngay sau đăng nhập:** để **mỗi tính năng thêm sau** đã có sẵn hàng rào quyền,
  không phải quay lại sửa.

### Bước 9 — Sản phẩm & danh mục (CRUD) + nhập tồn ban đầu
- **Làm gì:** `ProductService`/`ProductController`, `CategoryController`.
- **Tại sao trước đơn hàng:** **không có sản phẩm thì không đặt hàng được**. Đây là dữ liệu
  lõi mà đơn hàng tham chiếu.

### Bước 10 — Tồn kho an toàn (`InventoryService`) + sổ cái `GiaoDichKho`
- **Làm gì:** viết `ChangeStockCoreAsync` cập nhật tồn **có điều kiện** (`WHERE SoLuongTon >=
  quantity`) và ghi một dòng `GiaoDichKho`.
- **Tại sao trước đơn hàng:** đặt hàng = **trừ kho**. Phải có cơ chế trừ kho **chống bán lố**
  trước, rồi đơn hàng mới gọi vào. Sổ cái để đối soát về sau.

### Bước 11 — Giỏ hàng + Sổ địa chỉ
- **Làm gì:** `CartService`/`CartController`, `AddressService`/`AddressController`.
- **Tại sao:** là "nguyên liệu" cho bước đặt hàng của khách (giỏ → đơn; địa chỉ → người nhận).

### Bước 12 — Đơn hàng (`OrderService`) với transaction + idempotency + concurrency
- **Làm gì:** `CreateOrderAsync` (transaction: kiểm tồn → tạo đơn → trừ kho → tính tiền →
  lưu chi tiết), `UpdateStatusAsync` (concurrency version), `CancelOwnOrderAsync`.
- **Tại sao lúc này:** đơn hàng phụ thuộc **tất cả** phần trên (user, sản phẩm, kho, địa chỉ).
  Đây là nghiệp vụ trung tâm, làm khi nền đã vững.

### Bước 13 — Thanh toán (COD/VNPay/MoMo)
- **Làm gì:** `PaymentController` + `VnPayService`/`MoMoService`/`EmailService`. Xử lý
  callback/IPN, tự đẩy đơn sang `processing`, dọn giỏ khi thành công.
- **Tại sao sau đơn hàng:** thanh toán gắn với **một đơn đã tồn tại**.

### Bước 14 — Vận chuyển + báo cáo + audit/error log
- **Làm gì:** `ShippingService`/`ShippingController` (+ hosted service tự hoàn tất),
  `ReportController`, `AuditService`, `SystemErrorLogService`.
- **Tại sao sau cùng của backend:** là các lớp "quản trị/vận hành" phủ lên nghiệp vụ lõi.

### Bước 15 — Realtime (SignalR): `OrderHub`/`ChatHub` + `NotificationService`
- **Làm gì:** `AddSignalR`, map hub, đẩy sự kiện trong `NotificationService`, cấu hình token
  qua query cho WebSocket.
- **Tại sao gần cuối:** realtime **phủ lên** nghiệp vụ đã có (đổi trạng thái → đẩy). Phải có
  sự kiện để đẩy trước đã.

### Bước 16 — Client MAUI
- **Làm gì:** `dotnet new maui`, tạo `ApiClientProvider` (HttpClient + token), `LoginPage`,
  hai shell (`AppShell`/`CustomerShell`), rồi từng Page gọi API tương ứng; `SignalRService`
  cho realtime.
- **Tại sao sau backend:** client **tiêu thụ** API. Có API chạy (test bằng Swagger) rồi mới
  ghép giao diện — đỡ phải đoán.

### Bước 17 — Quan sát & vận hành
- **Làm gì:** `/health`, logging `[HTTP]`/`[Auth]`/`[Startup]`, xử lý lỗi tập trung
  (`ApiExceptionHandler`), script `scripts/*.ps1`.
- **Tại sao:** để **demo/gỡ lỗi** trơn tru — biết server sống không, request nào lỗi ở đâu.

---

## Sơ đồ phụ thuộc "cái gì cần cái gì"

```
PhanQuyen ─► NguoiDung ─► (JWT/Auth) ─► [mọi endpoint có quyền]
DanhMuc ─► SanPham ─► InventoryService(GiaoDichKho) ─┐
GioHang, DiaChi ────────────────────────────────────┼─► OrderService(DonHang, ChiTietDonHang)
                                                     │        │
                                                     │        ▼
                                                     │   ThanhToan ─► (VNPay/MoMo/COD, Email)
                                                     │        │
                                                     │        ▼
                                                     └──► VanChuyen ─► Report/Audit
                                                                     ▲
                                          NotificationService(SignalR) đẩy realtime ở mọi mốc
```

---

➡️ Tiếp theo: [09_Tinh_nang.md](09_Tinh_nang.md) — mổ xẻ từng tính năng theo luồng đầy đủ.
