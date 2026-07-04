# SyncChain — Sổ tay hướng dẫn phát triển (Developer Handbook)

> Tài liệu này **không phải báo cáo**. Đây là **sổ tay dành cho lập trình viên**: đọc
> xong, một người **chưa biết gì** về dự án vẫn có thể hiểu kiến trúc, hiểu vì sao mọi
> thứ được thiết kế như vậy, hiểu từng thư mục / file / thư viện / tính năng, và **tự
> dựng lại dự án từ đầu**.

## Dự án là gì (một câu)

**SyncChain** là hệ thống **client–server quản lý vận hành chuỗi bán lẻ**: nhân sự nội bộ
quản trị sản phẩm / kho / đơn hàng, khách hàng mua sắm qua một cổng riêng. Backend là
**ASP.NET Core Web API (.NET 10)**, client là **.NET MAUI** trên Windows, cơ sở dữ liệu
**PostgreSQL**, xác thực **JWT**, cập nhật thời gian thực bằng **SignalR**, thanh toán
**COD / VNPay / MoMo**. Đây là đồ án môn **NT106 – Lập trình mạng căn bản**.

## Cách đọc tài liệu này

Đọc **theo thứ tự** nếu bạn là người mới. Mỗi chương xây trên chương trước.

| # | File | Nội dung | Dành cho ai |
|---|------|----------|-------------|
| 1 | [01_Gioi_thieu.md](01_Gioi_thieu.md) | Dự án làm gì, có mấy module, mấy loại người dùng, ví dụ đời thực | Mọi người |
| 2 | [02_Kien_thuc_nen.md](02_Kien_thuc_nen.md) | Backend/Frontend/API/HTTP/REST/ORM/DI/JWT/SignalR — giải thích bằng ví dụ đời thực | Người mất gốc |
| 3 | [03_Chuan_bi_moi_truong.md](03_Chuan_bi_moi_truong.md) | Cài .NET SDK, workload MAUI, PostgreSQL, Git — cài để làm gì, thiếu thì lỗi gì | Người mới setup máy |
| 4 | [04_Kien_truc.md](04_Kien_truc.md) | Sơ đồ kiến trúc nhiều tầng, luồng một request đi từ đầu đến cuối | Muốn hiểu tổng thể |
| 5 | [05_Folder_Guide.md](05_Folder_Guide.md) | Vì sao cần từng thư mục, chứa gì, xóa đi thì sao | Muốn định vị code |
| 6 | [06_File_Guide.md](06_File_Guide.md) | Từng file quan trọng: vai trò, class, method, phụ thuộc, luồng dữ liệu | Muốn đọc code sâu |
| 7 | [07_Thu_vien.md](07_Thu_vien.md) | Từng thư viện: là gì, dùng ở đâu, không có thì phải tự viết gì | Muốn hiểu dependency |
| 8 | [08_Xay_dung_tu_dau.md](08_Xay_dung_tu_dau.md) | Dựng lại dự án từ số 0, từng bước, giải thích **tại sao** làm bước đó | Muốn build lại |
| 9 | [09_Tinh_nang.md](09_Tinh_nang.md) | Mổ xẻ từng tính năng theo luồng (đăng nhập, đặt hàng, thanh toán, realtime...) | Muốn hiểu nghiệp vụ |
| 10 | [10_Tu_duy_thiet_ke.md](10_Tu_duy_thiet_ke.md) | Tại sao tách Service/DTO/Interface, tại sao dùng DI, async, migration... | Muốn nâng trình |
| 11 | [11_Build_Run.md](11_Build_Run.md) | Clone → restore → build → chạy → test → gỡ lỗi thường gặp | Khi chạy thật |
| 12 | [12_Tong_ket.md](12_Tong_ket.md) | Tổng kết, checklist năng lực sau khi đọc | Cuối cùng |

## Ba điều cần nhớ trước khi bắt đầu

1. **Backend chính là `SyncChain.API` (.NET 10), không phải `src/` (Node).** Thư mục `src/`
   + `ui/` là **bản web prototype cũ** — tùy chọn, không nằm trong đường chạy của app MAUI.
   Đừng nhầm. (Chi tiết ở [01_Gioi_thieu.md](01_Gioi_thieu.md) mục "Hai phần chạy độc lập".)
2. **Không cần chạy migration tay.** Khi khởi động, backend **tự tạo schema + seed** dữ liệu
   (roles + tài khoản `admin@gmail.com` / `123456`). Xem `Program.cs`.
3. **Chuỗi kết nối DB đọc từ `.env` gốc** (`DATABASE_URL`), qua `EnvFileLoader` — **không**
   dùng `appsettings.json → ConnectionStrings`.

> Tài liệu tham chiếu gốc: [../README.md](../README.md) (bản tóm tắt) và
> [../README-DEV.md](../README-DEV.md) (quy trình dev). Sổ tay này đi **sâu hơn** nhiều.
