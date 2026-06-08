# 00 — Mẫu CMake C++17 (cross-platform) + Scripts

> Đề **bắt buộc**: dùng CMake, out-of-source build, build được trên Ubuntu + Windows (MSVC & MinGW64), `CMakeLists.txt` chạy **không cần sửa path/tasks.json**. File này cho bạn khung copy-paste.
>
> ⚠️ Đây là **mẫu in trong tài liệu** để bạn copy khi code. Ở giai đoạn hiện tại ta chưa tạo file CMake thật trong repo.

---

## 1. Root `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.20)
project(All6Labs LANGUAGES CXX)

# ---- C++17 bắt buộc, cross-platform ----
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)          # dùng -std=c++17 thay vì gnu++17 → đồng nhất MSVC/GCC

# Mặc định build Release nếu người dùng quên chọn (single-config generators)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Cảnh báo chặt (giúp bắt bug sớm)
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

enable_testing()                        # bật ctest

add_subdirectory(common)                # tiện ích dùng chung
add_subdirectory(lab1_aes_cryptopp)
add_subdirectory(lab2_aes_manual)
add_subdirectory(lab3_rsa_hybrid)
add_subdirectory(lab4_hash_pki)
add_subdirectory(lab5_signatures)
add_subdirectory(lab6_post_quantum)
```

---

## 2. `common/CMakeLists.txt` — thư viện tiện ích dùng chung

```cmake
add_library(common STATIC
    src/cli_parser.cpp
    src/codec.cpp          # hex / base64
    src/file_io.cpp        # đọc/ghi binary-safe (std::filesystem)
    src/kat_runner.cpp     # đọc JSON vectors, PASS/FAIL + summary
    src/benchmark.cpp      # warm-up + N runs + mean/median/stddev/CI
    src/rng.cpp            # wrapper RNG an toàn (AutoSeededRandomPool / RAND_bytes)
)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# JSON: dùng nlohmann_json (header-only) cho KAT runner
find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
  target_link_libraries(common PUBLIC nlohmann_json::nlohmann_json)
endif()

# Unit test cho common
add_subdirectory(tests)
```

---

## 3. Mẫu `labN/CMakeLists.txt` (ví dụ Lab 1 — Crypto++)

```cmake
# ---- Tìm Crypto++ (qua vcpkg) ----
find_package(cryptopp CONFIG REQUIRED)   # cung cấp target cryptopp::cryptopp

add_executable(aestool
    src/main.cpp
    src/aes_service.cpp
    src/key_manager.cpp
    src/nonce_manager.cpp
)
target_link_libraries(aestool PRIVATE common cryptopp::cryptopp)

# ---- Unit tests (Catch2 qua vcpkg) ----
find_package(Catch2 3 CONFIG REQUIRED)
add_executable(lab1_tests
    tests/test_aes_modes.cpp
    tests/test_negative.cpp
    tests/test_kat.cpp
)
target_link_libraries(lab1_tests PRIVATE common cryptopp::cryptopp Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(lab1_tests)
```

> **OpenSSL (Lab 3/4/5/6)** thay phần `find_package` bằng:
> ```cmake
> find_package(OpenSSL REQUIRED)            # OpenSSL::SSL OpenSSL::Crypto
> target_link_libraries(rsatool PRIVATE common OpenSSL::Crypto)
> ```
> **liboqs (Lab 6, nếu dùng)**:
> ```cmake
> find_package(liboqs CONFIG REQUIRED)
> target_link_libraries(pqtool PRIVATE common OQS::oqs)
> ```

---

## 4. `CMakePresets.json` — build không cần nhớ flag (đáp ứng "không sửa path")

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "win-msvc",
      "displayName": "Windows MSVC (vcpkg)",
      "generator": "Visual Studio 17 2022",
      "architecture": "x64",
      "binaryDir": "${sourceDir}/build/win-msvc",
      "cacheVariables": {
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      }
    },
    {
      "name": "linux-gcc",
      "displayName": "Linux GCC (vcpkg)",
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/linux-gcc",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_TOOLCHAIN_FILE": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
      }
    }
  ],
  "buildPresets": [
    { "name": "win-msvc",  "configurePreset": "win-msvc",  "configuration": "Release" },
    { "name": "linux-gcc", "configurePreset": "linux-gcc" }
  ]
}
```
Dùng:
```powershell
cmake --preset win-msvc
cmake --build --preset win-msvc
```

---

## 5. `vcpkg.json` (manifest mode) — đặt ở gốc repo

```json
{
  "name": "all6labs",
  "version-string": "1.0.0",
  "dependencies": [
    "cryptopp",
    "openssl",
    "nlohmann-json",
    { "name": "catch2", "version>=": "3.0.0" }
  ]
}
```
> Có manifest, chỉ cần truyền toolchain vcpkg là CMake **tự cài** đúng dependency khi configure. (Lab 6 thêm `"liboqs"` nếu dùng liboqs.)

---

## 6. Scripts

### `scripts/build_win.ps1`
```powershell
param([string]$Config = "Release")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

cmake -B "$root/build/win" -S $root -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build "$root/build/win" --config $Config
ctest --test-dir "$root/build/win" -C $Config --output-on-failure
```

### `scripts/build_linux.sh`
```bash
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

cmake -B "$ROOT/build/linux" -S "$ROOT" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
cmake --build "$ROOT/build/linux"
ctest --test-dir "$ROOT/build/linux" --output-on-failure
```

### `scripts/benchmark.ps1` (gọi tool benchmark → CSV)
```powershell
param([string]$Tool, [string]$OutCsv = "docs/plots/results.csv")
$ErrorActionPreference = "Stop"
# Power plan High performance để số liệu ổn định
powercfg /setactive SCHEME_MIN 2>$null
& $Tool --benchmark --csv $OutCsv
Write-Host "Saved $OutCsv"
```

### `scripts/make_plots.py` (vẽ chart từ CSV)
```python
import sys, csv
import matplotlib.pyplot as plt

path = sys.argv[1] if len(sys.argv) > 1 else "docs/plots/results.csv"
sizes, thr = [], []
with open(path, newline="") as f:
    for row in csv.DictReader(f):
        sizes.append(int(row["size"]))
        thr.append(float(row["throughput_mb_s"]))

plt.figure()
plt.plot(sizes, thr, marker="o")
plt.xscale("log", base=2)
plt.xlabel("Payload size (bytes)")
plt.ylabel("Throughput (MB/s)")
plt.title("AES throughput vs payload size")
plt.grid(True, which="both", linestyle="--", alpha=0.5)
plt.savefig("docs/plots/throughput.png", dpi=150, bbox_inches="tight")
print("Wrote docs/plots/throughput.png")
```

---

## 7. `.gitignore` tối thiểu
```gitignore
/build/
/build-*/
*.user
vcpkg_installed/
__pycache__/
docs/plots/*.png
```

---

## 8. Checklist CMake (chấm "Cross-platform build")
- [ ] `cmake -B build -S .` + `cmake --build build` chạy được, **không sửa path**
- [ ] Build OK trên MSVC, MinGW64, GCC(Linux)
- [ ] `ctest` pass cả 2 OS
- [ ] C++17 enforce (`CMAKE_CXX_STANDARD_REQUIRED ON`, `EXTENSIONS OFF`)
- [ ] Dùng target hiện đại (`cryptopp::cryptopp`, `OpenSSL::Crypto`) thay vì hardcode `-l`
- [ ] `vcpkg.json` liệt kê đủ deps + version

> Tiếp theo → [00_report_template.md](00_report_template.md).
