# Chương 12 — Tổng kết

## 12.1. Bạn đã đi qua những gì

- **Chương 1–2:** SyncChain là gì, hai phần chạy độc lập (ứng dụng chính .NET vs. prototype
  Node), 4 vai trò, và các khái niệm nền (Backend/API/HTTP/REST/ORM/DI/JWT/SignalR) qua ví
  dụ đời thực.
- **Chương 3–4:** cài môi trường và hiểu kiến trúc nhiều tầng + luồng một request từ đầu đến
  cuối (client → Controller → Service → EF Core → PostgreSQL → và realtime ngược lại).
- **Chương 5–6:** vai trò từng thư mục và từng file cốt lõi (Program, DbContext, Auth,
  Order, Inventory, Payment, Notification, Hub, các Model; phía MAUI: ApiClientProvider,
  SignalRService, App, LoginPage).
- **Chương 7:** các thư viện thật sự dùng (EF Core/Npgsql, JwtBearer, BCrypt, Swagger,
  SignalR, MAUI) và **vì sao KHÔNG dùng** AutoMapper/Serilog/MediatR/Identity...
- **Chương 8:** dựng lại dự án từ số 0 theo thứ tự phụ thuộc, giải thích "tại sao lúc này".
- **Chương 9:** từng tính năng theo luồng đầy đủ (đăng nhập, đặt hàng chống bán lố, thanh
  toán, đổi trạng thái chống xung đột, theo dõi realtime, kho, audit, chat).
- **Chương 10:** tư duy thiết kế và các đánh đổi (JWT, concurrency lạc quan, DTO, DI,
  `EnsureCreated` vs migration...).
- **Chương 11:** build, run, test, gỡ lỗi.

## 12.2. Checklist năng lực — sau khi đọc, bạn nên trả lời được

Tự kiểm tra. Nếu chỗ nào chưa chắc, quay lại chương tương ứng.

- [ ] Backend chính là gì, khác gì với `src/` (Node)? → C1, C5
- [ ] Một request đăng nhập đi qua những tầng/file nào? → C4, C9.1
- [ ] JWT được **tạo** ở đâu, **kiểm** ở đâu (file/hàm)? → C6 (AuthService, Program.cs)
- [ ] Mật khẩu lưu thế nào, vì sao không lưu chữ thật? → C2.8, C6 (BCrypt)
- [ ] SyncChain chống **bán lố** bằng cách nào (câu SQL nào)? → C6 (InventoryService), C9.4
- [ ] SyncChain chống **xung đột đổi trạng thái** bằng gì? → C6/C9.6/C10.6 (ConcurrencyVersion)
- [ ] **Idempotency** giải quyết vấn đề gì, cài đặt ra sao? → C9.4, C10.7
- [ ] Realtime hoạt động thế nào (Hub, Group, token qua query)? → C2.9, C6 (Hub/NotificationService)
- [ ] Vì sao tách Controller/Service/DTO, vì sao dùng DI/vòng đời? → C10
- [ ] Chuỗi kết nối DB đọc từ đâu, vì sao không để trong appsettings? → C6 (EnvFileLoader), C10.11
- [ ] Vì sao có cả `EnsureCreated` lẫn `Migrations/`, đánh đổi gì? → C10.12
- [ ] Các trạng thái đơn và luật chuyển hợp lệ? → C6 (OrderStatuses), C9.6
- [ ] Cách chạy toàn bộ và xử lý lỗi thường gặp? → C11

## 12.3. Những điểm dễ gây nhầm — ghi nhớ

1. **`src/` + `ui/` là prototype cũ (tùy chọn)**, không phải backend chính. Backend chính là
   `SyncChain.API` (.NET 10).
2. **DB config đọc từ `.env`**, không phải `appsettings.json → ConnectionStrings`.
3. **Không cần migration tay** để chạy — backend tự dựng schema + seed khi khởi động.
4. **Trạng thái đơn là lowercase** (`pending/processing/shipping/done/cancel`), nguồn chân
   lý ở `OrderStatuses.cs`. Bộ PascalCase (`Draft/Approved/...`) là **cũ/sai**.
5. **Quyết định phân quyền thật nằm ở server** (policy). Client chỉ ẩn/hiện menu.
6. **Giỏ hàng chỉ bị dọn khi thanh toán thành công**, không phải lúc tạo đơn.

## 12.4. Hướng phát triển thêm (gợi ý luyện tập)

Muốn thêm một tính năng mới, hãy đi theo mẫu đã học:

1. Thêm **Entity** (nếu cần bảng mới) + khai `DbSet<>` + quan hệ trong `AppDbContext`.
2. Thêm **DTO** cho request/response.
3. Viết **Service** chứa nghiệp vụ (transaction nếu ghi nhiều bảng; concurrency nếu có tranh
   chấp).
4. Thêm **Controller** + `[Authorize(Policy=...)]`; đăng ký Service ở `Program.cs`.
5. Nếu cần realtime → đẩy qua `NotificationService`/Hub.
6. Thêm **Page** ở MAUI gọi endpoint mới qua `ApiClientProvider.Client`.
7. Viết **script test** trong `scripts/` để kiểm.

Ví dụ bài tập tốt: "đánh giá sản phẩm (review)", "mã giảm giá (voucher)", "wishlist" —
mỗi cái đi trọn vòng Entity → DTO → Service → Controller → Page.

## 12.5. Tài liệu liên quan trong repo

- [../README.md](../README.md) — tóm tắt & hướng dẫn nhanh.
- [../README-DEV.md](../README-DEV.md) — quy trình dev, ví dụ log.
- [../CHAT_REALTIME.md](../CHAT_REALTIME.md) — chi tiết module chat realtime.
- [../HuongDan.md](../HuongDan.md) — hướng dẫn (nếu có).
- `SyncChain.API/Program.cs` — đọc file này là hiểu 60% backend.

---

**Kết:** Sau 12 chương, bạn không chỉ *biết code nằm ở đâu* mà hiểu *vì sao nó ở đó*. Đó là
khác biệt giữa "đọc source" và "nắm được hệ thống". Chúc bạn phát triển SyncChain vững vàng.
