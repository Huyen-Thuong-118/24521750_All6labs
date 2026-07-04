# Chương 9 — Phân tích từng tính năng (theo luồng)

Mỗi tính năng được mổ **theo luồng đầy đủ** — từ nút bấm ở client tới DB và ngược lại — kèm
tên file/method thật. Đây là chương để hiểu **hệ thống làm việc ra sao**.

---

## 9.1. Đăng nhập

```
[Client] LoginPage.OnLoginClicked → LoginAsync(customerShell)
   │  validate email/mật khẩu trống?
   ▼
[HTTP]  POST api/Auth/login  { email, password, device, location }
   ▼
[API]   AuthController.Login(LoginDTO)  →  AuthService.Login(dto)
   │     1. _db.NguoiDung.FirstOrDefault(email)         (EF Core → PostgreSQL)
   │     2. BCrypt.Verify(password, user.MatKhauHash)    → sai ⇒ audit LoginFailed + 401
   │     3. kiểm user.IsActive                            → khóa ⇒ 401 "Tai khoan bi khoa"
   │     4. tạo JWT (claims: user_id, name, email, role), hạn 2h, ký HmacSha256
   │     5. audit Login (kèm device/location)
   ▼
[HTTP]  200 { token, user { MaNguoiDung, TenDangNhap, Email, role } }
   ▼
[Client] kiểm role đúng cổng (customer↔CustomerShell; nội bộ↔AppShell)
   │  ApiClientProvider.SetSession(token, role, userId)   → gắn Bearer cho mọi request sau
   │  App.ShowShell() / App.ShowCustomerShell()
   ▼
[Client] SignalRService.StartAsync(token) → nối /hubs/order, JoinUserGroup(+Staff)
```
**Điểm mạng đáng nói:** token là "vé" stateless; sau khi có, **mọi** request tự kèm nó. Kiểm
role ở **cả** client (đúng cổng) **và** server (policy) — server là chốt chặn thật.

---

## 9.2. Đăng ký

```
[Client] RegisterPage → POST api/Auth/register { email, password, ho, ten, sdt, tenDangNhap? }
   ▼
[API] AuthController.Register → AuthService.Register
   │  chuẩn hóa email; kiểm trùng email; mật khẩu ≥ 6; lấy role "customer";
   │  BCrypt.HashPassword; thêm NguoiDung + audit  (TRONG transaction)
   ▼
200 "Dang ky thanh cong"  →  client mời đăng nhập
```
Chỉ tạo được **customer**; staff/manager do admin tạo (AdminController).

---

## 9.3. Duyệt sản phẩm → thêm giỏ

```
[Client] ProductsPage → GET api/Product (policy ProductRead: mọi role đăng nhập)
[Client] ProductDetailPage → "Thêm vào giỏ" → POST api/Cart (CartController → CartService)
   → ChiTietGioHang được thêm/tăng số lượng cho GioHang của user
```
Giá **luôn lấy từ DB** khi đặt hàng, không tin giá client hiển thị.

---

## 9.4. Đặt hàng (tính năng lõi — chống bán lố + idempotency + transaction)

```
[Client] CreateOrderPage → chọn địa chỉ (MaDiaChi), dịch vụ, ghi chú
   │  gửi kèm Idempotency-Key (header hoặc DTO) để chống double-submit
   ▼
[HTTP] POST api/Order (policy OrderWrite)  → OrderController.CreateOrder
   │  (nếu là nhân sự nội bộ: kiểm SalesChannel hợp lệ)
   ▼
[API] OrderService.CreateOrderAsync
   │  A. Có IdempotencyKey & đã tồn tại đơn ứng key ⇒ REPLAY (trả đơn cũ, KHÔNG tạo mới)
   │  B. Validate & gộp item; "Hỏa tốc" chỉ cho nội thành
   │  C. CreateOrderCoreAsync (TRANSACTION):
   │        - tải SanPham, kiểm tồn > 0 & không "Ngung ban"
   │        - ResolveRecipientAsync: ưu tiên sổ địa chỉ MaDiaChi của chính user
   │        - tạo DonHang (pending) + IdempotencyKey
   │        - với mỗi item: InventoryService.DecreaseStockAsync(OrderIssue, requireActiveProduct)
   │              → UPDATE SanPham SET SoLuongTon = SoLuongTon - q
   │                WHERE SoLuongTon >= q ...      ← CHỐNG BÁN LỐ (không trừ âm)
   │              → ghi 1 dòng GiaoDichKho (sổ cái)
   │        - tính tiền hàng; phí ship: đơn online SERVER tự tính (DeliveryEstimateService)
   │        - lưu ChiTietDonHang; audit; COMMIT
   │  D. Bắt lỗi: unique IdempotencyKey ⇒ replay; serialization/deadlock ⇒ 409 concurrency
   ▼
200 { MaDonHang, Subtotal, ShippingFee, TongTien }
```
**Ba lớp an toàn ở đây:**
1. **Transaction** — tạo đơn + trừ kho + ghi chi tiết là "tất cả hoặc không".
2. **Trừ kho có điều kiện** — chống hai khách mua món cuối cùng cùng lúc.
3. **Idempotency** — mạng chập chờn gửi 2 lần → chỉ 1 đơn (unique index + replay).

*Lưu ý thiết kế:* **KHÔNG xóa giỏ khi tạo đơn.** Đơn mới ở `pending` (chờ thanh toán); nếu
khách bỏ giữa chừng, sản phẩm vẫn còn trong giỏ để đặt lại. Giỏ chỉ bị dọn khi **thanh toán
thành công**.

---

## 9.5. Thanh toán

### COD
```
POST api/payment/initiate { MaDonHang, PhuongThuc: "cod" }
 → PaymentController: kiểm đơn pending, chưa thanh toán
 → ghi ThanhToan = Completed
 → OrderService.ClearPurchasedItemsAsync (DỌN GIỎ)
 → NotificationService.PushPaymentResultAsync (realtime) + EmailService (xác nhận)
 → đơn GIỮ pending để nhân sự xử lý
```
### VNPay / MoMo (online)
```
POST api/payment/initiate { PhuongThuc: "vnpay"|"momo" }
 → tạo ThanhToan = Pending, sinh paymentUrl (VnPayService/MoMoService)
 → client mở trình duyệt sandbox → người dùng thanh toán
 → cổng gọi callback/IPN: /api/payment/{vnpay|momo}/{return|ipn}
      → VALIDATE CHỮ KÝ (không tin resultCode suông)
      → cập nhật ThanhToan = Completed/Failed
      → nếu thành công:
           OrderService.AdvanceToProcessingAfterPaymentAsync  (pending → processing, idempotent)
           OrderService.ClearPurchasedItemsAsync (dọn giỏ)
           Email + realtime PushPaymentResult
 → return trả trang HTML "Thành công/Thất bại" để đóng và quay lại app
```
**Điểm mạng/bảo mật:** kết quả thật đến từ **IPN server-to-server có chữ ký**, không dựa vào
redirect mà người dùng có thể giả mạo.

---

## 9.6. Đổi trạng thái đơn (chống xung đột đồng thời) — trọng tâm môn mạng

```
[Client nhân sự] OrdersPage → chọn trạng thái mới
   │  gửi kèm expectedStatus + concurrencyVersion (đọc được lúc xem đơn)
   ▼
PUT api/Order/{id}/status (policy OrderManage) → OrderService.UpdateStatusAsync
   │  1. snapshot = (TrangThai, ConcurrencyVersion) hiện tại
   │  2. đơn đã terminal (done/cancel) ⇒ 409 OrderAlreadyProcessed
   │  3. status/version hiện tại ≠ expected ⇒ 409 ConcurrencyConflict
   │  4. OrderStatuses.CanTransition(cur, req)? nếu không hợp lệ ⇒ 400
   │  5. UPDATE DonHang SET TrangThai=req, ConcurrencyVersion+1
   │        WHERE id=.. AND TrangThai=expected AND ConcurrencyVersion=expected
   │        → changedRows phải = 1; nếu 0 ⇒ ai đó đã đổi trước ⇒ 409
   │  6. nếu cancel ⇒ RestoreCancelledOrderStockAsync (HOÀN KHO)
   │  7. audit + COMMIT
   ▼
NotificationService.PushOrderStatusAsync → realtime tới chủ đơn + nhóm staff
```
**Vì sao đây là bài toán mạng kinh điển:** hai nhân viên mở cùng đơn, cùng bấm đổi. Nhờ
`ConcurrencyVersion` trong điều kiện WHERE, **chỉ một người thắng**; người kia nhận 409 và
được yêu cầu tải lại. Không có cơ chế này → "lost update" (mất cập nhật của người kia).

---

## 9.7. Khách tự hủy đơn

```
PUT api/Order/{id}/cancel  → OrderService.CancelOwnOrderAsync
   │  chỉ đơn CỦA CHÍNH user và đang pending
   │  chặn nếu đã thanh toán online Completed (yêu cầu liên hệ hoàn tiền)
   │  UPDATE có điều kiện version → hoàn kho → audit → realtime "Đã hủy"
```

---

## 9.8. Theo dõi đơn (timeline realtime)

```
GET api/Order/{id}/tracking → trả { order, payment, chiTiet, timeline }
   timeline = BuildTrackingTimeline(status):
       pending → processing → shipping → done  (đánh dấu hoanThanh/hienTai/choDoi)
       hoặc pending(hoanThanh) → cancel(huyBo)
[Client] OrderTrackingPage vẽ timeline; đồng thời lắng nghe SignalR "OrderStatusUpdated"
         → khi nhân sự đổi trạng thái, timeline tự nhích KHÔNG cần F5.
```

---

## 9.9. Quản lý kho (nhập/xuất/điều chỉnh/đối soát)

```
Phiếu nhập  → WarehouseReceiptService → InventoryService.IncreaseStockAsync(Receipt)
Phiếu xuất  → WarehouseIssueService   → InventoryService.DecreaseStockAsync(ManualIssue)
Điều chỉnh  → InventoryController      → InventoryService.AdjustStockAsync (có lý do + audit)
Đối soát    → InventoryService.ReconcileStockAsync: so tồn hiện tại vs. tổng sổ cái GiaoDichKho
             → báo chênh lệch, tùy chọn đồng bộ
```
Mọi thay đổi kho **đều để lại vết** ở `GiaoDichKho` → truy vết được "vì sao tồn thay đổi".

---

## 9.10. Realtime & thông báo (SignalR)

- **Server → client:** `NotificationService` vừa **lưu `ThongBao`** (để xem lại) vừa
  **`SendAsync`** qua `OrderHub` tới nhóm `user_<id>` / `staff`.
- **Client:** `SignalRService` bắt `OrderStatusUpdated`, `PaymentResult`, `NewNotification`
  → `BeginInvokeOnMainThread` cập nhật UI. `WithAutomaticReconnect()` tự nối lại khi rớt.
- **Xác thực WebSocket:** token qua `?access_token=`; backend nhận ở `OnMessageReceived`.

---

## 9.11. Nhật ký (Audit) & lỗi hệ thống (SystemErrorLog)

- **AuditLog:** ai làm gì (create/update/login/order-status-change/inventory-adjustment...),
  trước/sau (jsonb), truy vết `TraceId`. Ghi rải rác trong các Service qua `IAuditService`.
- **SystemErrorLog:** lỗi validation/401/403/exception được ghi tự động (xem `Program.cs`
  `InvalidModelStateResponseFactory`, `UseStatusCodePages`, `ApiExceptionHandler`).
- Xem qua `AuditLogsController` / `SystemErrorLogsController` (chỉ admin).

---

## 9.12. Chat nội bộ realtime (mở rộng)

- `ChatHub` (`/hubs/chat`) + `ChatService`/`ChatController` + các bảng `Chat*` (hội thoại,
  thành viên, tin nhắn, poll, bình chọn). Hỗ trợ nhóm, ghim, thu hồi, reaction, đính kèm file
  (lưu `wwwroot/uploads/chat`). Đây là phần realtime thứ hai, độc lập với đơn hàng.

---

➡️ Tiếp theo: [10_Tu_duy_thiet_ke.md](10_Tu_duy_thiet_ke.md) — vì sao thiết kế như vậy.
