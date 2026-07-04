# Chương 11 — Build & Run (chi tiết)

Hướng dẫn từ lúc **clone** tới lúc **đăng nhập chạy được**, kèm bảng sự cố thường gặp. Nội
dung dựa trên `README.md`, `README-DEV.md`, thư mục `scripts/` và các file `run-*`.

## 11.1. Yêu cầu trước khi bắt đầu

| Bắt buộc cho ứng dụng chính | Kiểm tra |
|-----------------------------|----------|
| .NET SDK 10.0.x | `dotnet --version` |
| Workload MAUI | `dotnet workload list` (nếu thiếu: `dotnet workload install maui`) |
| PostgreSQL (Neon **hoặc** local) | mục 11.3 |
| Windows 10/11 (cho MAUI) | — |

(Tùy chọn) Node.js 18+ nếu muốn chạy prototype `src/`.

## 11.2. Clone & restore

```powershell
git clone <repo-url>
cd NT106_SyncChain
dotnet restore                # tải NuGet cho cả solution
```
- **Tại sao restore:** tải các package (`Npgsql`, `JwtBearer`, `BCrypt`, `Swashbuckle`,
  MAUI, SignalR client...) khai trong các `.csproj`. Thiếu bước này → build báo thiếu package.

## 11.3. Cấu hình database (`.env`)

```powershell
Copy-Item .env.example .env
```
Sửa `.env`, để **đúng một** dòng `DATABASE_URL` không phải comment:
```env
# Neon (cloud) — không cần cài PostgreSQL local:
DATABASE_URL=postgresql://<user>:<password>@<host>.neon.tech/syncchain?sslmode=require

# Hoặc local (comment dòng trên, bỏ comment dòng này):
# DATABASE_URL=postgresql://postgres:<password>@localhost:5432/syncchain
```
- **Local:** phải **tạo sẵn database rỗng** trước: `CREATE DATABASE syncchain;` (backend tự
  tạo *bảng*, không tự tạo *database*).
- **Neon:** dùng ngay, không cài gì.
- Kiểm nhanh kết nối: `.\scripts\run-database.ps1`.

## 11.4. Cách nhanh nhất — chạy tất cả

```powershell
.\run-all.bat        # hoặc: .\scripts\run-all.ps1   (hoặc alias .\run.ps1)
```
Script thực hiện đúng thứ tự:
1. Kiểm PostgreSQL (đọc `DATABASE_URL`, thử kết nối).
2. Chạy **backend** trong cửa sổ riêng.
3. **Chờ tới khi `GET /health` trả 200** (DB nối OK, schema/seed xong).
4. Chạy **app Desktop** ở cửa sổ hiện tại.

## 11.5. Chạy từng phần

```powershell
# Chỉ backend
.\run-backend.bat
#   API:     http://localhost:5292
#   Health:  http://localhost:5292/health
#   Swagger: http://localhost:5292/swagger

# Chỉ client MAUI
.\run-frontend.bat
#   Nếu backend chưa chạy: màn Đăng nhập báo "Máy chủ chưa sẵn sàng" (không crash)

# Chỉ kiểm DB
.\scripts\run-database.ps1
```

### Chạy thủ công (không script)
```powershell
dotnet run --project SyncChain.API
# cửa sổ khác:
dotnet run --project app/SyncChain.Desktop -f net10.0-windows10.0.19041.0
```
Hoặc mở `NT106_SyncChain.sln` bằng Visual Studio 2022, nhấn **F5**.

## 11.6. Đăng nhập lần đầu

| Vai trò | Tài khoản | Cổng |
|---------|-----------|------|
| admin | `admin@gmail.com` / `123456` (seed sẵn) | nút **Đăng nhập** |
| customer | tự **Đăng ký** trong app | nút **Đăng nhập khách hàng** |
| staff/manager | do admin tạo | nút **Đăng nhập** |

## 11.7. Kiểm chứng nhanh (smoke test)

1. Mở `http://localhost:5292/health` → `{ "status":"healthy", "database":"connected" }`.
2. Mở `http://localhost:5292/swagger` → thử `POST api/Auth/login` với admin → nhận token.
3. Đăng ký một customer trong app → đăng nhập cổng khách → đặt thử một đơn.
4. Đăng nhập admin → đổi trạng thái đơn → thấy app khách cập nhật realtime.

## 11.8. Test tự động (tùy chọn)

Thư mục `scripts/` có sẵn kịch bản kiểm thử: `test-oversell.ps1` (chống bán lố),
`test-order-conflicts.ps1` (xung đột trạng thái), `test-shipping.ps1`, `test-reports.ps1`,
`test-audit-logs.ps1`, `test-chat.ps1`, `test-system-error-logs.ps1`, và `test-core-e2e.mjs`.
Chạy khi backend đang chạy để kiểm các luồng chính.

## 11.9. Lỗi thường gặp & cách xử lý

| Hiện tượng | Nguyên nhân / xử lý |
|------------|---------------------|
| Backend thoát ngay khi khởi động | `DATABASE_URL` sai/thiếu, hoặc DB không nối được. Local: nhớ `CREATE DATABASE syncchain`. |
| `Program.cs` ném "Chua cau hinh DATABASE_URL" | `.env` thiếu dòng `DATABASE_URL` không-comment. |
| `/health` trả 503 | DB mất kết nối (Neon ngủ/đổi mật khẩu/hết hạn). |
| Màn Đăng nhập báo "Máy chủ chưa sẵn sàng" | Backend chưa chạy — chạy `run-backend.bat`. |
| `run-all` báo "Backend did not become healthy" | Xem cửa sổ backend: lỗi thật (DB/JWT/cổng) nằm ở đó. |
| Cổng 5292 bận | Còn tiến trình `SyncChain.API` cũ — đóng cửa sổ/kết thúc tiến trình. |
| Build MAUI báo `.exe` bị khóa | App đang chạy — đóng app Desktop rồi build lại. |
| Build báo thiếu workload maui | `dotnet workload install maui`. |
| Đăng nhập báo "Sai thông tin" ngay sau đăng ký | Phải đăng ký **trong app** (gọi API thật), và đúng cổng theo vai trò. |
| Đổi trạng thái báo 409 | Ai đó đã đổi trước (concurrency) — tải lại đơn rồi thử lại. Đây là hành vi **đúng thiết kế**. |

## 11.10. Đổi địa chỉ backend cho client

Client mặc định gọi `http://localhost:5292/`. Đổi bằng biến môi trường trước khi chạy app:
```powershell
$env:SYNCCHAIN_API_URL = "http://192.168.1.10:5292/"
.\run-frontend.bat
```
(Đọc bởi `ApiClientProvider.ApiBaseUrl`.)

## 11.11. Quan sát log khi demo

- **Backend:** `[Startup]` (kết nối/seed/sẵn sàng), `[HTTP] <METHOD> <path> -> <status>
  (<ms>)`, `[Auth]` (đăng ký/đăng nhập/sinh JWT).
- **Client:** `[Desktop/Login]`, `[Desktop/Register]`... trong Debug output.
- **SignalR:** `[SignalR] Connected...` khi client nối hub.

---

➡️ Tiếp theo: [12_Tong_ket.md](12_Tong_ket.md) — tổng kết & checklist năng lực.
