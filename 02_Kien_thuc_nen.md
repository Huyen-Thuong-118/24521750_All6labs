# Chương 2 — Kiến thức nền (giải thích cho người mất gốc)

Chương này giải thích các khái niệm bằng **ví dụ đời thực**, rồi chỉ **ngay trong mã
SyncChain** chỗ nào minh họa khái niệm đó. Đọc chương này xong, các chương sau sẽ dễ.

## 2.1. Backend là gì? Frontend là gì?

Hãy hình dung một **nhà hàng**:

- **Frontend = phòng ăn + người phục vụ.** Đây là phần khách nhìn thấy và bấm vào. Trong
  SyncChain, frontend là **app MAUI** (`app/SyncChain.Desktop`) — các màn hình Đăng nhập,
  Sản phẩm, Giỏ hàng...
- **Backend = nhà bếp + kho.** Khách không vào bếp; khách chỉ **gọi món**, bếp nấu rồi
  đưa ra. Trong SyncChain, backend là **`SyncChain.API`** — nơi kiểm tra mật khẩu, trừ kho,
  ghi đơn vào database. Khách (app) **không bao giờ** đụng trực tiếp vào database; app chỉ
  "gọi món" qua API.

**Vì sao tách hai phần?** Vì bếp (logic + dữ liệu) phải **an toàn và dùng chung**. Nếu để
app tự nối thẳng vào database, mỗi máy khách sẽ biết mật khẩu DB, tự ý sửa dữ liệu, và
không ai kiểm soát được luật (ví dụ "không được bán quá tồn kho"). Đặt luật ở backend →
**một nơi duy nhất giữ luật**, mọi client phải tuân theo.

## 2.2. API là gì?

**API = thực đơn (menu) của nhà bếp.** Nó liệt kê "bạn được gọi những món gì, gọi thế nào,
sẽ nhận lại gì". Khách không cần biết bếp nấu ra sao; chỉ cần biết: *"Gọi `POST
api/Auth/login` kèm email + mật khẩu → nhận lại một token."*

Trong SyncChain, mỗi **endpoint** (mục trên thực đơn) là một method trong Controller. Ví dụ
trong `AuthController.cs`:

```csharp
[HttpPost("login")]                       // ← địa chỉ món: POST api/Auth/login
public IActionResult Login([FromBody] LoginDTO dto)  // ← nguyên liệu khách đưa vào
{
    var result = _auth.Login(dto);        // ← bếp nấu
    return Ok(result);                    // ← món trả ra (JSON có token)
}
```

App gọi đúng địa chỉ đó (xem `LoginPage.xaml.cs`):
```csharp
var response = await _http.PostAsJsonAsync("api/Auth/login", new { email, password, ... });
```

## 2.3. HTTP là gì?

**HTTP = ngôn ngữ để gọi món qua điện thoại.** Mỗi cuộc gọi có:

- **Method (động từ):** bạn muốn *làm gì*.
  - `GET` = "cho tôi xem" (lấy dữ liệu, không thay đổi gì). VD: `GET api/Order` = xem đơn.
  - `POST` = "tạo mới". VD: `POST api/Order` = tạo đơn.
  - `PUT` = "cập nhật". VD: `PUT api/Order/5/status` = đổi trạng thái đơn 5.
  - `DELETE` = "xóa".
- **URL (địa chỉ):** gọi tới đâu. VD: `http://localhost:5292/api/Order/5`.
- **Headers (ghi chú kèm):** VD `Authorization: Bearer <token>` = "tôi là ai" (xem 2.7).
- **Body (nội dung):** dữ liệu gửi kèm, thường là **JSON**. VD `{ "email": "...", "password": "..." }`.
- **Status code (mã kết quả):** bếp trả về *200* (OK), *400* (bạn gửi sai), *401* (chưa
  đăng nhập), *403* (không có quyền), *404* (không tìm thấy), *409* (xung đột), *500* (bếp
  lỗi). SyncChain dùng đúng các mã này — xem `Program.cs` và các Controller.

Backend còn **log lại mọi cuộc gọi** (xem `Program.cs` dòng ~433):
```
[HTTP] POST /api/Auth/login -> 200 (37 ms)
```

## 2.4. REST là gì?

**REST** là một **phong cách đặt thực đơn cho gọn và nhất quán**: gom theo *tài nguyên*
(danh từ) và dùng *method* (động từ) để phân biệt hành động. Thay vì đặt tên lung tung
kiểu `layDonHang`, `taoDonHangMoi`, `xoaDon`, REST quy ước:

| Muốn gì | REST |
|---------|------|
| Xem danh sách đơn | `GET /api/Order` |
| Xem 1 đơn | `GET /api/Order/5` |
| Tạo đơn | `POST /api/Order` |
| Đổi trạng thái đơn | `PUT /api/Order/5/status` |
| Khách tự hủy đơn | `PUT /api/Order/5/cancel` |

SyncChain theo đúng phong cách này (xem `OrderController.cs`). Nhờ nhất quán, người mới
**đoán được** endpoint mà không cần tra cứu.

## 2.5. ORM là gì? (Entity Framework Core)

Database nói ngôn ngữ **SQL** (`SELECT * FROM "DonHang" WHERE ...`). C# nói ngôn ngữ **đối
tượng** (`class DonHang`). **ORM = người phiên dịch** giữa hai bên. Bạn viết C#, ORM tự dịch
sang SQL và ngược lại.

- Không có ORM, bạn phải tự viết SQL bằng tay và tự "ghép" kết quả vào object (giống bản
  prototype `src/index.js` dùng `pool.query('INSERT INTO ...')`).
- Có ORM (**EF Core** trong SyncChain), bạn viết:
  ```csharp
  var user = _db.NguoiDung.FirstOrDefault(x => x.Email == email);  // EF tự dịch thành SELECT
  _db.DonHang.Add(order);                                          // EF nhớ để INSERT
  _db.SaveChanges();                                               // EF chạy SQL thật
  ```
- **Entity** = một class ánh xạ với một bảng. VD `NguoiDung.cs` ↔ bảng `NguoiDung`.
- **DbContext** (`AppDbContext.cs`) = "phiên dịch viên trưởng": khai báo có những bảng nào
  (`DbSet<...>`) và cấu hình quan hệ giữa chúng.

## 2.6. Repository Pattern & Dependency Injection

### Repository Pattern (mẫu kho chứa)
Ý tưởng: **giấu chi tiết truy cập dữ liệu sau một lớp trung gian**, để phần nghiệp vụ không
cần biết dữ liệu nằm ở PostgreSQL hay MongoDB.

> **Ghi chú trung thực về SyncChain:** dự án **không** tạo lớp `Repository` riêng. Thay vào
> đó, **`DbContext` của EF Core đã đóng vai trò "repository"** (bản thân `DbSet<T>` là một
> repository + Unit of Work sẵn có). Tầng **Service** (VD `OrderService`) gọi thẳng
> `_db.DonHang...`. Đây là lựa chọn phổ biến và hợp lý cho quy mô đồ án — xem lý giải ở
> [10_Tu_duy_thiet_ke.md](10_Tu_duy_thiet_ke.md#tại-sao-không-có-repository-riêng).

### Dependency Injection — DI (tiêm phụ thuộc)
**Ví dụ đời thực:** bạn pha cà phê nhưng **không tự đi trồng cà phê**; có người **giao**
gói cà phê tới cho bạn. Bạn chỉ nói "tôi cần cà phê", ai đó lo phần cung cấp.

Trong code: `OrderService` **cần** `AppDbContext`, `InventoryService`, `IAuditService`... Nó
**không tự tạo** chúng, mà **khai báo ở constructor** và để hệ thống "tiêm" vào:

```csharp
public OrderService(AppDbContext db, InventoryService inventory,
                    IAuditService audit, DeliveryEstimateService deliveryEstimate,
                    INotificationService notify)  // ← chỉ khai báo "tôi cần các thứ này"
{
    _db = db; _inventory = inventory; ...        // ← nhận và cất đi
}
```

Ai "tiêm"? Đó là **DI container**, được cấu hình ở `Program.cs`:
```csharp
builder.Services.AddScoped<OrderService>();       // "khi ai cần OrderService, hãy tạo nó"
builder.Services.AddScoped<AppDbContext>(...);     // và biết cách tạo các thứ nó cần
```

**Lợi ích:** dễ thay thế (test có thể tiêm bản giả), không rối "ai tạo ai", và quản lý
**vòng đời** (Scoped = mỗi request một bản; Singleton = một bản dùng chung cả app). Xem chi
tiết vòng đời ở [10_Tu_duy_thiet_ke.md](10_Tu_duy_thiet_ke.md).

## 2.7. JWT là gì?

**Ví dụ đời thực: vé xem phim có dập chống giả.** Khi mua vé (đăng nhập), rạp đưa bạn một
**vé** ghi rõ *ghế, suất chiếu* và **đóng dấu nổi** chỉ rạp mới làm được. Sau đó mỗi lần vào
cửa, bạn chìa vé; nhân viên **không cần gọi về quầy hỏi lại**, chỉ nhìn con dấu là biết vé
thật. Vé cũng ghi **giờ hết hạn**.

**JWT (JSON Web Token)** chính là "vé" đó cho web:

- **Đăng nhập → nhận token.** Backend tạo token gồm các *claim* (thông tin): `user_id`,
  tên, email, **role**. Xem `AuthService.Login` (`AuthService.cs` dòng ~123–147):
  ```csharp
  var claims = new[]
  {
      new Claim("user_id", user.MaNguoiDung.ToString()),
      new Claim(ClaimTypes.Role, roleName),          // ← vai trò nằm trong vé
      ...
  };
  var token = new JwtSecurityToken(issuer:..., audience:..., claims:claims,
                                   expires: DateTime.Now.AddHours(2),  // ← vé hết hạn sau 2h
                                   signingCredentials: creds);         // ← "đóng dấu" bằng khóa bí mật
  ```
- **Mỗi request kèm token.** App gắn header `Authorization: Bearer <token>`. Trong MAUI,
  `ApiClientProvider.SetSession` gắn một lần cho `HttpClient` dùng chung.
- **Backend kiểm token, không cần hỏi lại DB.** Cấu hình ở `Program.cs` dòng ~118–143:
  kiểm issuer, audience, **hạn dùng**, và **chữ ký** bằng khóa `Jwt:Key`.

**Không dùng JWT thì sao?** Bạn phải giữ **session ở server** (server nhớ ai đang đăng
nhập). Với nhiều client/nhiều server sẽ khó chia sẻ trạng thái, và mỗi request phải tra
cứu. JWT **không trạng thái (stateless)**: mọi thông tin nằm trong vé → hợp với hệ thống
mạng nhiều client. **Nơi tạo JWT:** `AuthService.Login`. **Nơi kiểm JWT:** middleware
`UseAuthentication()` cấu hình trong `Program.cs`.

> **Lưu ý bảo mật quan trọng:** vì role nằm trong token và token *không hỏi lại DB*, nên
> khi khóa/đổi quyền một tài khoản, token cũ vẫn hợp lệ **tối đa 2 giờ** (tới khi hết hạn).
> Đây là đánh đổi cố hữu của JWT.

## 2.8. BCrypt là gì? (băm mật khẩu)

**Không bao giờ lưu mật khẩu dạng chữ thật** trong database. Nếu DB bị lộ, mọi mật khẩu
mất sạch. Thay vào đó ta lưu **bản băm** (hash) — một chiều, không thể đảo ngược.

- Đăng ký: `MatKhauHash = BCrypt.Net.BCrypt.HashPassword(password)` (`AuthService.cs` ~58).
- Đăng nhập: `BCrypt.Net.BCrypt.Verify(password, user.MatKhauHash)` (`AuthService.cs` ~94)
  — băm lại mật khẩu vừa nhập rồi so với bản băm đã lưu.

BCrypt còn tự thêm **salt** (muối ngẫu nhiên) và **cố tình chạy chậm** để chống dò mật
khẩu hàng loạt.

## 2.9. SignalR là gì? (realtime)

**Ví dụ đời thực: app gọi xe.** Khi tài xế nhận cuốc, bạn **không phải bấm F5 liên tục** —
màn hình tự cập nhật "Tài xế đang đến". Đó là **server chủ động đẩy** thông tin xuống bạn.

- **Cách cũ (polling):** client cứ vài giây hỏi server "có gì mới không?" → tốn mạng, chậm.
- **Cách của SignalR (WebSocket):** mở **một đường ống hai chiều** luôn mở giữa client và
  server. Khi có sự kiện, server **gửi thẳng** xuống client.

Trong SyncChain:
- Server có 2 **Hub**: `OrderHub` (`/hubs/order`) cho đơn/thanh toán/thông báo, và
  `ChatHub` (`/hubs/chat`) cho chat nội bộ. Đăng ký ở `Program.cs`:
  `app.MapHub<OrderHub>("/hubs/order")`.
- Khi trạng thái đơn đổi, `NotificationService.PushOrderStatusAsync` gọi:
  ```csharp
  await _hub.Clients.Group($"user_{userId}").SendAsync("OrderStatusUpdated", orderId, status, ts);
  ```
  → chỉ **đúng khách sở hữu đơn** (nhóm `user_<id>`) và nhóm `staff` nhận được.
- Client MAUI lắng nghe trong `SignalRService.cs`:
  ```csharp
  _conn.On<int, string, string>("OrderStatusUpdated", (orderId, status, ts) => ...);
  ```

**JWT đi kèm SignalR thế nào?** WebSocket không gắn header dễ như HTTP, nên token được
truyền qua query string `?access_token=...`; backend xử lý ở `Program.cs` dòng ~131–142
(`OnMessageReceived` — nếu path bắt đầu bằng `/hubs` thì lấy token từ query).

## 2.10. Các khái niệm ngắn còn lại

- **JSON:** định dạng text để trao đổi dữ liệu, kiểu `{ "key": "value" }`. HTTP body của
  SyncChain đều là JSON.
- **DTO (Data Transfer Object):** "tờ khai" định nghĩa *đúng những trường* đi vào/ra qua
  API, tách khỏi Entity (bảng). VD `LoginDTO`, `CreateOrderDTO`. Vì sao cần DTO → xem
  [10_Tu_duy_thiet_ke.md](10_Tu_duy_thiet_ke.md).
- **Middleware:** các "trạm kiểm soát" mà mỗi request đi qua theo thứ tự (log → xử lý lỗi →
  xác thực → phân quyền → tới Controller). Xem chuỗi `app.Use...` cuối `Program.cs`.
- **Transaction:** "làm tất cả hoặc không làm gì". VD tạo đơn gồm nhiều bước (ghi đơn, trừ
  kho, ghi chi tiết); nếu một bước lỗi thì **hoàn tác toàn bộ** để dữ liệu không dở dang.
  SyncChain dùng `BeginTransactionAsync()` khắp `OrderService`/`InventoryService`.
- **Concurrency (đồng thời):** nhiều người cùng sửa một dữ liệu. SyncChain chống bằng
  **ConcurrencyVersion** (số phiên bản tăng dần) + trừ kho có điều kiện — xem
  [09_Tinh_nang.md](09_Tinh_nang.md).
- **Idempotency (tính bất biến khi lặp):** gọi API tạo đơn 2 lần (do mạng chập chờn) chỉ
  tạo **một** đơn, nhờ `IdempotencyKey`. Xem `OrderService.CreateOrderAsync`.

---

➡️ Tiếp theo: [03_Chuan_bi_moi_truong.md](03_Chuan_bi_moi_truong.md) — cài đặt công cụ,
mỗi thứ cài để làm gì và thiếu thì lỗi ra sao.
