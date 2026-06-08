# 00 — Dựng môi trường phát triển trên Windows 11

> Mục tiêu: sau file này bạn có thể **build + chạy + debug + test** một project CMake C++17 link tới **Crypto++** và **OpenSSL** trên Windows 11, và biết cách verify mọi thứ qua PATH. Mọi lệnh dùng **PowerShell**.

> ✅ **Nguyên tắc vàng:** Cài đúng thứ tự — Visual Studio → CMake → Git → vcpkg → (vcpkg cài Crypto++/OpenSSL). Sau mỗi bước **verify version** rồi mới qua bước sau.

---

## 1. Visual Studio 2022 (MSVC + toolchain C++)

1. Tải **Visual Studio 2022 Community** (miễn phí): <https://visualstudio.microsoft.com/>
2. Trong **Visual Studio Installer**, chọn workload:
   - ✅ **Desktop development with C++**
3. Đảm bảo các component (thường đã kèm workload trên):
   - MSVC v143 - VS 2022 C++ x64/x86 build tools
   - **C++ CMake tools for Windows** (kèm CMake + Ninja)
   - Windows 11 SDK
   - C++ AddressSanitizer (tùy chọn, hữu ích để debug)

**Verify** — mở **"Developer PowerShell for VS 2022"** (Start menu) rồi:
```powershell
cl            # MSVC compiler — phải in ra "Microsoft (R) C/C++ Optimizing Compiler ..."
where cl      # đường dẫn tới cl.exe
```
> ⚠️ `cl` chỉ có trong **Developer PowerShell/Command Prompt** (đã nạp biến môi trường MSVC). PowerShell thường sẽ không thấy `cl`.

---

## 2. CMake

VS2022 đã kèm CMake. Nếu muốn bản hệ thống (khuyến nghị, để chạy ngoài VS):

**Cách A — winget (nhanh nhất):**
```powershell
winget install Kitware.CMake
```
**Cách B — tải installer:** <https://cmake.org/download/> (nhớ chọn *Add CMake to PATH*).

**Verify:**
```powershell
cmake --version      # cần >= 3.20 (đề khuyến nghị mới)
```
Nếu báo "không nhận lệnh" → mở terminal mới (PATH cập nhật sau khi cài).

---

## 3. Git

```powershell
winget install Git.Git
```
**Verify:**
```powershell
git --version
```

---

## 4. Ninja (optional — build nhanh hơn)

Đã kèm trong VS workload. Bản hệ thống:
```powershell
winget install Ninja-build.Ninja
ninja --version
```

---

## 5. vcpkg — quản lý dependency (Crypto++, OpenSSL)

`vcpkg` là package manager C/C++ của Microsoft, tích hợp mượt với CMake/MSVC. Đây là cách **đơn giản & ít lỗi nhất** để có Crypto++ và OpenSSL.

### 5.1. Cài vcpkg
```powershell
# Cài vào C:\ cho gọn (tránh path có dấu cách)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
```

### 5.2. Đặt biến môi trường (1 lần)
```powershell
# Thêm vĩnh viễn cho user hiện tại
[Environment]::SetEnvironmentVariable("VCPKG_ROOT", "C:\vcpkg", "User")
# Mở terminal MỚI để biến có hiệu lực, rồi kiểm tra:
$env:VCPKG_ROOT
```

### 5.3. Cài thư viện (triplet x64-windows)
```powershell
C:\vcpkg\vcpkg install cryptopp:x64-windows
C:\vcpkg\vcpkg install openssl:x64-windows
# (Lab 6) nếu cần Post-Quantum qua liboqs:
C:\vcpkg\vcpkg install liboqs:x64-windows
```
> 💡 **Manifest mode (khuyến nghị cho repo):** thay vì cài global, tạo `vcpkg.json` ở gốc repo liệt kê dependency → ai clone về cũng cài đúng version. Xem [00_cmake_template.md](00_cmake_template.md) §6.

**Verify đã cài:**
```powershell
C:\vcpkg\vcpkg list        # phải thấy cryptopp, openssl (và liboqs nếu cài)
```

### 5.4. Cho CMake "thấy" vcpkg
Khi cấu hình CMake, truyền **toolchain file**:
```powershell
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```
(Hoặc dùng `CMakePresets.json` — xem template.)

---

## 6. (Thay thế) MinGW64 — vì đề yêu cầu build được cả MSVC **và** MinGW64

Đề yêu cầu compile trên **Windows (MSVC và MinGW64)**. Bạn nên kiểm thử cả hai. Cài MinGW64 qua **MSYS2**:
```powershell
winget install MSYS2.MSYS2
```
Mở **"MSYS2 MINGW64"** shell rồi:
```bash
pacman -Syu                                   # cập nhật (có thể phải mở lại shell)
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-crypto++ mingw-w64-x86_64-openssl
```
**Verify** (trong MINGW64 shell):
```bash
gcc --version
cmake --version
```
> ⚠️ Triplet vcpkg cho MinGW là `x64-mingw-dynamic`/`x64-mingw-static` — khác `x64-windows`. Để đơn giản, trên MinGW dùng package của pacman như trên thay vì vcpkg.

---

## 7. Build / Run / Test / Debug — lệnh mẫu

Giả sử bạn đang ở thư mục gốc repo (có `CMakeLists.txt`).

### 7.1. Build (out-of-source — đúng yêu cầu đề)
```powershell
# Cấu hình (chỉ rõ generator + toolchain vcpkg)
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake

# Build (Release)
cmake --build build --config Release

# (Tùy chọn) Ninja thay vì VS generator — build nhanh:
cmake -B build-ninja -S . -G Ninja `
      -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake
cmake --build build-ninja
```

### 7.2. Run
```powershell
# Ví dụ Lab 1 (đường dẫn binary tùy generator)
.\build\lab1_aes_cryptopp\Release\aestool.exe encrypt --mode gcm `
    --key-hex 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f `
    --text "hello" --encode hex
```

### 7.3. Test (ctest — đề yêu cầu ctest pass)
```powershell
ctest --test-dir build -C Release --output-on-failure
```

### 7.4. Debug
- **Trong Visual Studio:** `File > Open > CMake...` → chọn `CMakeLists.txt` gốc → đặt breakpoint → F5.
- **Trong VS Code:** cài extension *C/C++* + *CMake Tools* → chọn kit MSVC → "CMake: Debug".
- **Dòng lệnh (cdb/WinDbg):** chỉ khi cần; thường IDE đủ dùng.

---

## 8. Kiểm tra PATH & sự cố thường gặp

| Triệu chứng | Nguyên nhân | Cách xử lý |
|-------------|-------------|-----------|
| `cmake`/`git` "not recognized" | PATH chưa cập nhật | Mở terminal **mới**; verify bằng `cmake --version` |
| `cl` không tìm thấy | Không ở Developer PowerShell | Dùng *Developer PowerShell for VS 2022* |
| CMake không thấy Crypto++/OpenSSL | Thiếu toolchain vcpkg | Thêm `-DCMAKE_TOOLCHAIN_FILE=...vcpkg.cmake` |
| Link lỗi `unresolved external` | Sai triplet (x86 vs x64) hoặc thiếu `target_link_libraries` | Dùng `x64-windows`, kiểm tra CMake link |
| DLL not found khi chạy | DLL vcpkg chưa ở cạnh .exe | vcpkg tự copy khi dùng toolchain; nếu Ninja, set `VCPKG_APPLOCAL_DEPS` hoặc dùng static triplet |
| Build OK Win, fail Linux | Dùng API Windows-only / path `\` | Dùng `std::filesystem`, tránh `<windows.h>` trong core |

**Lệnh xem PATH hiện tại:**
```powershell
$env:Path -split ';'
```

---

## 9. Checklist môi trường (tick trước khi code)

- [ ] `cl` chạy được trong Developer PowerShell (MSVC OK)
- [ ] `cmake --version` ≥ 3.20
- [ ] `git --version` OK
- [ ] `$env:VCPKG_ROOT` trỏ đúng `C:\vcpkg`
- [ ] `vcpkg list` thấy `cryptopp`, `openssl`
- [ ] (Lab 6) `vcpkg list` thấy `liboqs` **hoặc** có OpenSSL ≥ 3.5
- [ ] (Cross-platform) MinGW64/MSYS2 build thử OK
- [ ] Build out-of-source thành công + `ctest` pass

> Sau khi xong môi trường → [00_cmake_template.md](00_cmake_template.md) để có khung CMake dùng cho mọi lab.
