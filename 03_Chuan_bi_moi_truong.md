# Chương 3 — Chuẩn bị môi trường

Mỗi công cụ dưới đây được **giải thích: cài để làm gì, dự án dùng nó ở đâu, thiếu thì lỗi
gì**. Cài theo thứ tự.

## 3.1. .NET SDK 10

- **Là gì:** bộ công cụ để **biên dịch và chạy** mọi thứ C# trong dự án (cả `SyncChain.API`
  lẫn app MAUI). SDK gồm trình biên dịch, thư viện runtime, và lệnh `dotnet`.
- **Dự án dùng ở đâu:** `SyncChain.API.csproj` khai `<TargetFramework>net10.0</TargetFramework>`;
  app MAUI nhắm `net10.0-windows10.0.19041.0`. Toàn bộ được build/run bằng `dotnet build`,
  `dotnet run`.
- **Kiểm tra:** `dotnet --version` → phải ra `10.0.x`.
- **Thiếu thì lỗi gì:** máy không hiểu lệnh `dotnet`; hoặc báo *"The current .NET SDK does
  not support targeting .NET 10.0"* nếu SDK cũ hơn.

## 3.2. Workload MAUI (`maui` / `maui-windows`)

- **Là gì:** **gói bổ sung** cho .NET để dựng ứng dụng **.NET MAUI** (giao diện đa nền
  tảng). SDK gốc *chưa* đủ để build MAUI; phải cài thêm workload này.
- **Dự án dùng ở đâu:** `app/SyncChain.Desktop` là dự án MAUI. Trên Windows nó build ra app
  desktop (unpackaged).
- **Cài:** `dotnet workload install maui` — kiểm tra bằng `dotnet workload list`.
- **Thiếu thì lỗi gì:** build app báo *"workload 'maui'/'maui-windows' is not installed"*
  hoặc không nạp được các SDK `Microsoft.Maui.*`.

> **Chỉ chạy backend thì có cần MAUI không?** Không. Nếu bạn chỉ chạy `SyncChain.API` (ví
> dụ để test API qua Swagger/Postman), bạn **không cần** workload MAUI.

## 3.3. PostgreSQL (Neon cloud **hoặc** local)

- **Là gì:** hệ quản trị **cơ sở dữ liệu** nơi lưu mọi dữ liệu (user, sản phẩm, đơn, kho...).
- **Dự án dùng ở đâu:** backend nối tới qua **Npgsql** bằng chuỗi `DATABASE_URL` trong
  `.env`. Có hai lựa chọn:
  - **Neon (cloud):** PostgreSQL serverless, không cần cài gì trên máy. Chỉ dán chuỗi kết
    nối vào `.env`. Nhanh nhất cho đồ án nhóm.
  - **Local:** cài PostgreSQL trên máy, rồi **tạo sẵn database rỗng**: `CREATE DATABASE
    syncchain;`. Backend sẽ tự tạo bảng khi khởi động (`EnsureCreated()`).
- **Kiểm tra:** `.\scripts\run-database.ps1` xác nhận kết nối được.
- **Thiếu / sai thì lỗi gì:** cửa sổ backend **tắt ngay khi khởi động** kèm lỗi kết nối; hoặc
  `Program.cs` ném *"Chua cau hinh DATABASE_URL..."* nếu `.env` thiếu biến. Với local mà chưa
  `CREATE DATABASE syncchain` → lỗi *database "syncchain" does not exist*.

## 3.4. Git

- **Là gì:** công cụ **quản lý phiên bản mã nguồn** — clone dự án về, theo dõi thay đổi,
  làm việc nhóm (nhánh, pull request).
- **Dự án dùng ở đâu:** clone repo, tạo nhánh, commit. Dự án có sẵn lịch sử commit và các PR.
- **Kiểm tra:** `git --version`.
- **Thiếu thì:** không clone/không commit được (nhưng nếu đã có mã nguồn thì vẫn build chạy
  bình thường).

## 3.5. NuGet (đi kèm .NET SDK)

- **Là gì:** kho **thư viện** cho .NET (giống "chợ ứng dụng" cho developer). Khai báo trong
  file `.csproj` phần `<PackageReference>`, tải về bằng `dotnet restore`.
- **Dự án dùng ở đâu:** `SyncChain.API.csproj` khai `BCrypt.Net-Next`,
  `Microsoft.AspNetCore.Authentication.JwtBearer`, `Npgsql.EntityFrameworkCore.PostgreSQL`,
  `Microsoft.EntityFrameworkCore.Tools`, `Swashbuckle.AspNetCore`. Xem chi tiết ở
  [07_Thu_vien.md](07_Thu_vien.md).
- **Thiếu / offline thì lỗi gì:** `restore` thất bại → build báo thiếu package.

## 3.6. Entity Framework Core Tools (tùy chọn)

- **Là gì:** bộ lệnh `dotnet ef ...` để **tạo/áp dụng migration** (thay đổi schema DB có
  kiểm soát).
- **Dự án dùng ở đâu:** khai trong `.csproj` (`Microsoft.EntityFrameworkCore.Tools`). Thư
  mục `SyncChain.API/Migrations/` chứa các migration đã có.
- **Có bắt buộc không:** **Không**, để *chạy* dự án. Vì khi khởi động, backend dùng
  `EnsureCreated()` + các câu `ALTER TABLE ... IF NOT EXISTS` để tự dựng schema (xem
  `Program.cs`). Bạn chỉ cần EF Tools khi muốn **tự tạo migration mới** trong lúc phát triển.

## 3.7. Node.js 18+ (chỉ khi chạy bản prototype `src/`)

- **Là gì:** môi trường chạy JavaScript ngoài trình duyệt.
- **Dự án dùng ở đâu:** **chỉ** cho bản web prototype `src/` (Express) — **không** cần cho
  app MAUI. `package.json` khai `express`, `pg`, `cors`, `dotenv`.
- **Thiếu thì:** không chạy được `npm start` cho prototype; **không ảnh hưởng** ứng dụng chính.

## 3.8. IDE (chọn một)

- **Visual Studio 2022:** mở `NT106_SyncChain.sln`, nhấn **F5** để build+chạy. Tốt nhất cho
  MAUI trên Windows.
- **VS Code / Rider:** dùng được, kết hợp lệnh `dotnet` và các script PowerShell trong
  `scripts/`.

## 3.9. Docker (không bắt buộc)

Dự án **không yêu cầu Docker**. Nếu muốn, bạn có thể chạy PostgreSQL trong container thay vì
cài local, nhưng đó chỉ là lựa chọn hạ tầng DB — phần còn lại không đổi.

## 3.10. Bảng tổng hợp "thiếu cái gì → triệu chứng gì"

| Thiếu / sai | Triệu chứng |
|-------------|-------------|
| .NET SDK 10 | `dotnet` không chạy / báo không hỗ trợ net10.0 |
| Workload MAUI | Build `app/SyncChain.Desktop` báo thiếu workload |
| `.env` / `DATABASE_URL` | Backend ném *"Chua cau hinh DATABASE_URL"* rồi thoát |
| DB không kết nối được | Cửa sổ backend tắt ngay; `/health` trả 503 |
| DB local chưa tạo `syncchain` | Lỗi *database does not exist* |
| Cổng 5292 bận | Backend không mở được; còn tiến trình cũ |
| App đang chạy khi build lại | Build MAUI báo file `.exe` bị khóa |

---

➡️ Tiếp theo: [04_Kien_truc.md](04_Kien_truc.md) — sơ đồ kiến trúc nhiều tầng và luồng một
request từ đầu đến cuối.
