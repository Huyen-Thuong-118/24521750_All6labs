# Chương 1 — Giới thiệu dự án

## 1.1. SyncChain giải quyết vấn đề gì?

Hãy tưởng tượng một **chuỗi cửa hàng bán lẻ** (ví dụ bán đồ công nghệ). Cửa hàng đó có
hai nhóm người cần một phần mềm:

- **Người trong cửa hàng (nhân sự nội bộ):** cần biết còn bao nhiêu hàng trong kho, nhập
  thêm hàng, xuất hàng, xem đơn khách đặt, duyệt đơn, giao hàng, xem doanh thu, xem ai
  đã làm gì (nhật ký).
- **Khách hàng:** cần xem sản phẩm, bỏ vào giỏ, đặt mua, chọn địa chỉ giao, thanh toán,
  rồi **theo dõi đơn** của mình đang tới đâu.

Nếu làm thủ công (sổ sách, Excel, Zalo), sẽ xảy ra các vấn đề kinh điển:

- **Bán lố (oversell):** hai khách cùng mua món cuối cùng trong kho → cửa hàng chốt cả
  hai đơn → không đủ hàng giao. SyncChain **chống bán lố** bằng transaction + trừ kho có
  điều kiện (xem [09_Tinh_nang.md](09_Tinh_nang.md)).
- **Không biết đơn tới đâu:** khách gọi điện hỏi "đơn của tôi sao rồi?". SyncChain có
  **timeline realtime**: nhân sự đổi trạng thái → khách thấy ngay trong app (SignalR).
- **Không biết ai sửa gì:** SyncChain ghi **AuditLog** (nhật ký thao tác) và
  **SystemErrorLog** (nhật ký lỗi hệ thống).

> **Một câu:** SyncChain là phần mềm để **một cửa hàng vừa quản trị nội bộ, vừa bán hàng
> online cho khách**, chạy theo mô hình **client–server** (nhiều máy khách cùng nối tới
> một máy chủ) — đúng trọng tâm môn *Lập trình mạng*.

## 1.2. Hai phần chạy độc lập (RẤT QUAN TRỌNG)

Kho mã nguồn chứa **hai hệ thống**, dễ gây nhầm cho người mới:

### (A) Ứng dụng chính — bản để demo & chấm điểm
```
app/SyncChain.Desktop  (client .NET MAUI, Windows)
        │  HTTP + JWT, SignalR
        ▼
SyncChain.API          (backend ASP.NET Core .NET 10, cổng 5292)
        │  Npgsql
        ▼
PostgreSQL
```
Đây là thứ bạn nên tập trung. **95% tài liệu này nói về phần (A).**

### (B) Bản web prototype — tùy chọn, là "bản nháp" đầu tiên
```
ui/    (HTML/CSS/JS thuần)
   │
   ▼
src/   (Node.js + Express, cổng 3000)
   │  pg
   ▼
PostgreSQL (cùng DB)
```
`src/` + `ui/` là **phiên bản đầu tay** của đồ án, viết bằng Node để làm quen. Nó **không
bắt buộc** để chạy app MAUI và **không** phải backend chính. Bằng chứng ngay trong mã:
`src/index.js` tự ghi chú *"API thật nằm ở src/server.js — file này chỉ để thử logic dưới
DB"*, và `package.json` đặt tên vỏn vẹn là `"backend"` với vài dependency Node cơ bản
(`express`, `pg`, `cors`, `dotenv`).

> **Quy ước trong sổ tay:** khi nói "backend" mà không ghi chú gì thêm, ta luôn nói tới
> **`SyncChain.API` (.NET)**. Khi nói tới bản Node sẽ ghi rõ "bản prototype `src/`".

Cả hai phần **dùng chung một database PostgreSQL** và cùng đọc `DATABASE_URL` trong `.env`.

## 1.3. Có bao nhiêu loại người dùng (vai trò)?

Backend định nghĩa **4 vai trò**, seed sẵn trong bảng `PhanQuyen` (xem `Program.cs`
dòng ~394):

| MaVaiTro | Tên role | Là ai | Đăng nhập qua |
|----------|----------|-------|---------------|
| 1 | `customer` | Khách hàng | nút **"Đăng nhập khách hàng"** → `CustomerShell` |
| 2 | `staff` | Nhân viên | nút **"Đăng nhập"** (quản trị) → `AppShell` |
| 3 | `manager` | Quản lý | nút **"Đăng nhập"** → `AppShell` |
| 4 | `admin` | Quản trị hệ thống | nút **"Đăng nhập"** → `AppShell` |

- `customer` **tự đăng ký** trong app.
- `admin` được **seed sẵn**: `admin@gmail.com` / `123456` (xem `Program.cs` dòng ~411).
- `staff` / `manager` do `admin` tạo trong mục Quản lý người dùng.

Quyền được thực thi ở backend bằng **policy theo role** (khai báo trong `Program.cs`, ví dụ
`OrderManage` = staff/manager/admin, `ProductWrite` = manager/admin...). Client chỉ *ẩn/hiện
menu*; **quyết định thật nằm ở server**.

## 1.4. Có bao nhiêu module?

Nhìn theo Controller của backend (mỗi Controller ~ một nhóm chức năng):

| Module | Controller | Ý nghĩa |
|--------|-----------|---------|
| Xác thực | `AuthController` | Đăng ký, đăng nhập, hồ sơ, đổi mật khẩu |
| Sản phẩm | `ProductController`, `CategoryController` | CRUD sản phẩm, danh mục |
| Kho | `InventoryController`, `WarehouseReceiptsController`, `WarehouseIssuesController` | Tồn kho, phiếu nhập, phiếu xuất |
| Giỏ hàng | `CartController` | Giỏ hàng của khách |
| Đơn hàng | `OrderController` | Tạo đơn, đổi trạng thái, hủy, theo dõi |
| Thanh toán | `PaymentController` | COD / VNPay / MoMo |
| Vận chuyển | `ShippingController` | Tạo vận đơn, cập nhật giao hàng |
| Địa chỉ | `AddressController` | Sổ địa chỉ giao hàng của khách |
| Thông báo | `NotificationController` | Danh sách thông báo trong app |
| Chat | `ChatController` (+ `ChatHub`) | Chat nội bộ realtime |
| Báo cáo | `ReportController` | Doanh thu, thống kê |
| Nhật ký | `AuditLogsController`, `SystemErrorLogsController` | Audit + error log |
| Quản trị user | `AdminController` | Tạo/khóa/đặt lại mật khẩu nhân sự |

Mỗi module đi theo mẫu **Controller → Service → EF Core → PostgreSQL** (xem
[04_Kien_truc.md](04_Kien_truc.md)).

## 1.5. Business hoạt động ra sao? (kịch bản một khách hàng thật)

Ví dụ bạn tên **Lan**, muốn mua bàn phím:

```
1. Lan mở app → bấm "Đăng ký" → nhập email/mật khẩu → có tài khoản customer.
2. Lan "Đăng nhập khách hàng" → app nhận JWT, mở CustomerShell (giao diện khách).
3. Lan xem trang chủ, chọn "Bàn phím cơ", bấm "Thêm vào giỏ".
4. Lan mở Giỏ hàng → bấm "Đặt hàng" → chọn địa chỉ giao (từ sổ địa chỉ của Lan).
5. Server tạo đơn ở trạng thái "pending" (chờ duyệt) + TRỪ KHO ngay (giữ hàng cho Lan).
6. Lan chọn thanh toán COD → server ghi nhận, xóa món đã mua khỏi giỏ, gửi email xác nhận.
7. Nhân viên cửa hàng đăng nhập quản trị → thấy đơn của Lan → bấm "Đang xử lý".
8. NGAY LẬP TỨC, app của Lan (đang mở) nhận thông báo realtime (SignalR): "Đơn #.. Đang xử lý".
9. Nhân viên tạo vận đơn → trạng thái "shipping" → Lan thấy timeline nhích tới "Đang giao".
10. Giao xong → "done". Nếu Lan đổi ý lúc đơn còn "pending", Lan tự bấm Hủy → kho được hoàn lại.
```

Mọi bước ở trên đều có thật trong mã và được mổ xẻ chi tiết ở
[09_Tinh_nang.md](09_Tinh_nang.md).

## 1.6. Vì sao đề tài này hợp với môn "Lập trình mạng"?

Trọng tâm chấm điểm không phải "giao diện đẹp" mà là **các bài toán mạng**:

- **Nhiều client đồng thời** nối tới một server (client MAUI + có thể nhiều máy).
- **Đẩy dữ liệu thời gian thực** từ server xuống client (SignalR/WebSocket) — không phải
  client cứ hỏi liên tục.
- **Xử lý xung đột khi truy cập đồng thời**: hai người đổi trạng thái một đơn cùng lúc,
  hai người mua món cuối cùng cùng lúc → giải quyết bằng **transaction + ConcurrencyVersion
  + trừ kho có điều kiện**.
- **Xác thực & phân quyền qua mạng** bằng **JWT** (token gắn kèm mỗi request).

Những điểm này lặp lại xuyên suốt tài liệu, đặc biệt ở [09_Tinh_nang.md](09_Tinh_nang.md)
và [10_Tu_duy_thiet_ke.md](10_Tu_duy_thiet_ke.md).

---

➡️ Tiếp theo: [02_Kien_thuc_nen.md](02_Kien_thuc_nen.md) — giải thích các khái niệm nền
(Backend, API, HTTP, REST, ORM, DI, JWT, SignalR) bằng ví dụ đời thực, cho người mất gốc.
