# 00 — Tổng quan toàn môn: Cryptography & Applications (Labs 1–6)

> **Tài liệu này dành cho ai?** Sinh viên (MSV `24521750`) làm chuỗi 6 lab Cryptography trên **Windows 11**, build bằng **CMake + C++17 (MSVC/VS2022)**, có thể build lại trên Linux. Mục tiêu: **đạt tối đa rubric** và nộp một báo cáo chuyên nghiệp.
>
> **Cách dùng bộ guide:** Đọc theo thứ tự trong [README.md](README.md). 4 file `00_*.md` là kiến thức nền dùng chung cho mọi lab; 6 file `labN_*.md` là hướng dẫn chi tiết từng lab theo 12 mục.

---

## 1. Bức tranh tổng thể

Đề bài (`Cryptography_Labs.pdf`) là **một loạt 6 lab** liên kết với nhau, nộp trong **một file nén duy nhất** tên `MSV.All6labs.rar` (ví dụ `24521750.All6labs.rar`), kèm **một báo cáo duy nhất** (~60 trang) bao trùm cả 6 lab.

| Lab | Chủ đề | Thư viện bắt buộc | Điểm nhấn |
|-----|--------|-------------------|-----------|
| **Lab 1** | Symmetric Encryption với Crypto++ | **Chỉ Crypto++** (cấm OpenSSL/libsodium) | 8 chế độ AES, AEAD, IV/nonce lifecycle, nonce-reuse detection, sidecar JSON |
| **Lab 2** | AES-128 thuần C++ (CTR, FIPS-197) | **Không dùng thư viện crypto nào** | Tự code AES round functions + KeyExpansion + CTR, cross-validate với Crypto++/OpenSSL |
| **Lab 3** | RSA-OAEP & Hybrid Encryption | Crypto++ **hoặc** OpenSSL | RSA-3072 OAEP(SHA-256), hybrid envelope AES-256-GCM |
| **Lab 4** | Hashing, PKI & Practical Attacks | OpenSSL hoặc Crypto++ | SHA-2/SHA-3/SHAKE, X.509 parsing, MD5 collision, length-extension (offline) |
| **Lab 5** | Classical Digital Signatures | Crypto++ **+** OpenSSL | ECDSA-P256 (RFC 6979), RSA-PSS-3072 |
| **Lab 6** | Post-Quantum Signatures & Certificates | OpenSSL 3.5+ / liboqs | ML-DSA-44, ML-KEM-512, PQ certificate mini-project |

> 💡 **Liên kết quan trọng:** Lab 2 phải cho ra **ciphertext y hệt** Lab 1/OpenSSL (cross-validation). Lab 3 dùng lại AES-GCM (giống Lab 1). Lab 6 phải **so sánh** với Lab 5. Vì vậy **đừng code rời rạc** — hãy tái sử dụng module chung (xem `common/`).

---

## 2. Mục tiêu học tập của cả môn (Course Objectives)

Theo đề, sau 6 lab sinh viên phải:

1. Implement & phân tích các cryptographic primitive hiện đại.
2. Áp dụng **secure engineering** trong tool CLI.
3. Kiểm chứng tính đúng đắn bằng **NIST test vectors** (KAT).
4. Phát hiện & ngăn chặn **misuse** (vd: nonce reuse).
5. Benchmark hiệu năng **đúng phương pháp thống kê**.
6. Viết **báo cáo kỹ thuật chuyên nghiệp**.

---

## 3. Common Engineering Requirements (ÁP DỤNG CHO MỌI LAB)

> Đây là phần **dễ mất điểm nhất** vì nó không nằm trong từng lab mà rải đều. In phần này ra dán lên tường.

### 3.1. Build & cross-platform
- **Bắt buộc dùng CMake**, hỗ trợ **out-of-source build**:
  ```bash
  mkdir build && cd build
  cmake ..
  cmake --build .
  ```
- Phải compile được trên: **Ubuntu LTS**, **Windows (MSVC và MinGW64)**.
- `CMakeLists.txt` phải build **không cần sửa path hay tasks.json**.

### 3.2. README.md (top-level) phải có
- Danh sách dependency **kèm version**.
- Hướng dẫn cài đặt (Windows & Linux).
- Lệnh build cho cả 2 OS.
- Ví dụ sử dụng CLI.
- Known limitations.

### 3.3. CLI chuẩn (THỐNG NHẤT mọi lab)
Mọi tool đều theo cấu trúc:
```
mytool <command>
       [--in INFILE | --text "…"]
       [--out OUTFILE]
       [--key KEYFILE | --key-hex HEX]
       [--iv IV-hex]
       [--nonce NONCE-hex]
       [--mode MODE]
       [--aead]
       [--encode hex|base64|raw]
       [--threads N]
       [--kat path/to/vectors.json]
       [--verbose]
```
Yêu cầu:
- Nhận input **UTF-8**, xuất output **UTF-8**.
- **Fail closed** khi input sai (không bao giờ "đoán" rồi chạy tiếp).

### 3.4. Input/Output & Encoding
- **Input:** `--in file` (binary-safe) hoặc `--text "..."` (UTF-8).
- **Output:** `--out file` (binary-safe). Mặc định:
  - Màn hình → **hex** cho ciphertext/digest/signature.
  - File → **raw binary** (trừ khi override).
- **Encoding:** `--encode hex | base64 | raw`.

### 3.5. Randomness & Key Management
- **PHẢI dùng:** `AutoSeededRandomPool` (Crypto++) hoặc `RAND_bytes` (OpenSSL).
- **CẤM dùng:** `rand()`, `std::random_*` cho mục đích crypto. ❗ Đây là lỗi trừ điểm rất phổ biến.
- IV/Nonce: enforce đúng length, ngăn reuse khi cấm, auto-generate an toàn nếu thiếu, **document format lưu trữ** (header/sidecar).
- **Key size tối thiểu:**

  | Thuật toán | Yêu cầu |
  |------------|---------|
  | AES | 128/192/256-bit (báo cáo nên ưu tiên **256**) |
  | RSA | **≥ 3072-bit** |
  | ECDSA | **≥ P-256 (secp256r1)** |

### 3.6. Verification & Testing
- **KAT:** mỗi lab có NIST test vectors + một `--kat` runner đọc **JSON**, in **PASS/FAIL từng case** + **summary**.
- **Negative testing:** wrong key, tampered ciphertext, tampered AEAD tag, malformed input, corrupted PEM/key — tất cả phải **fail securely**.
- **Unit test:** dùng **Catch2 hoặc GoogleTest**, `ctest` phải pass trên Windows & Linux. CI là optional.

### 3.7. Performance Methodology (CHUẨN HÓA — copy vào mọi lab)
1. **Warm-up:** 1–2 giây (ổn định cache/allocator).
2. Chạy block ~1.000 operations.
3. Lặp lại **N ≥ 30** (đến 100) lần độc lập.
4. Thu thập thống kê: **Mean, Median, Standard deviation, 95% Confidence Interval**.
5. **Fairness:** pin CPU governor (Linux: `performance`; Windows: power plan **High performance**).
6. **Repeatability:** chỉ fix PRNG seed cho **dữ liệu synthetic**, **KHÔNG BAO GIỜ** fix seed cho key/nonce.
7. So sánh: Windows vs Linux, biến thể thuật toán, mode overhead, tác động hardware acceleration.
8. Output: **bảng + biểu đồ có error bar + diễn giải** (không chỉ số liệu thô).

### 3.8. Security Engineering Standards (mỗi report phải bàn)
- Threat model
- Misuse scenarios
- Known attacks liên quan thuật toán
- Secure defaults
- Fail-closed behavior
- Limitations của implementation

### 3.9. Deliverables Checklist (file nén cuối cùng)
- [ ] Source code (cả 6 lab)
- [ ] CMake files
- [ ] README.md (top-level)
- [ ] Unit tests
- [ ] **Binary Windows**
- [ ] **Binary Linux**
- [ ] Report (PDF/DOCX) — **1 file cho cả 6 lab**
- [ ] Self-grade checklist (bảng rubric tự chấm)
- [ ] Academic integrity statement

### 3.10. Academic Integrity (dán vào report — Appendix A)
- Tuyên bố tính nguyên bản, cite thư viện/KAT/tài liệu.
- Demo tấn công **chỉ trên artifact của mình, sandbox offline**.
- Không test lên hệ thống/dịch vụ bên thứ ba.
- Key/secret trong repo chỉ là **test keys**.
- Nếu dùng AI/tool hỗ trợ → **khai báo**.

### 3.11. Optional GUI Bonus (mọi lab)
- Export crypto core thành `.dll/.so/.a/.lib`.
- GUI bằng Python (PySide6/Qt) hoặc C# (WPF/WinUI), **gọi lại thư viện đã compile**, **không lặp lại logic crypto**.

---

## 4. Cấu trúc repository đề xuất

> Đề **không bắt buộc** layout cụ thể (chỉ yêu cầu CMake out-of-source, README top-level, 1 report). Layout dưới đây gom theo từng lab cho dễ chấm và dễ tái sử dụng `common/`.

```
24521750.All6Labs/
├── README.md                  # Top-level: deps + version, install, build, CLI usage, limitations
├── CMakeLists.txt             # Root: add_subdirectory cho common + 6 lab
├── CMakePresets.json          # (nên có) preset cho Win/Linux để build không sửa path
│
├── common/                    # ❗ Code dùng chung cho nhiều lab (TRÁI TIM của repo)
│   ├── include/               #   - CLI parser, hex/base64 codec, file I/O, KAT runner,
│   ├── src/                   #     benchmark framework, RNG wrapper, error handling
│   └── tests/                 #   - unit test cho tiện ích chung
│
├── docs/
│   ├── guide/                 # 📘 Bộ hướng dẫn này (bạn đang đọc)
│   ├── specs/                 # Để file đề Cryptography_Labs.pdf ở đây
│   ├── report/                # Báo cáo cuối (1 file .docx/.pdf cho cả 6 lab)
│   ├── screenshots/           # Ảnh chụp build/run/test/TLS để chèn report
│   └── plots/                 # Biểu đồ benchmark (.png) sinh từ CSV
│
├── third_party/               # Thư viện nhúng (nếu không dùng vcpkg): vendored deps
│
├── scripts/
│   ├── build_win.ps1          # Build trên Windows (cấu hình + build + ctest)
│   ├── build_linux.sh         # Build trên Linux
│   ├── benchmark.ps1          # Chạy benchmark → CSV
│   └── make_plots.py          # Vẽ biểu đồ từ CSV (matplotlib)
│
├── lab1_aes_cryptopp/
│   ├── src/  include/         # Code & header của lab
│   ├── tests/                 # Unit/integration/negative test
│   ├── kat/                   # File JSON test vectors (NIST)
│   ├── benchmark/             # Code đo + CSV output
│   └── resources/             # File mẫu để mã hóa/giải mã
│
├── lab2_aes_manual/           # (giống cấu trúc trên)
├── lab3_rsa_hybrid/
├── lab4_hash_pki/
│   └── certificates/          # Cert mẫu (.pem) để parse, cert tự ký test
├── lab5_signatures/
├── lab6_post_quantum/
│
└── build/                     # Out-of-source build (KHÔNG commit; thêm vào .gitignore)
```

### Vai trò từng thư mục
- **`common/`** — Nơi đặt mọi tiện ích dùng lại: argument parser, codec hex/base64, đọc/ghi file binary-safe, KAT runner đọc JSON, benchmark framework (warm-up + N runs + thống kê), RNG wrapper an toàn, định nghĩa error/exception "fail closed". Mỗi lab `link` vào `common` → tránh copy-paste, giảm bug.
- **`docs/`** — Mọi thứ phục vụ báo cáo: guide, đề gốc, report, ảnh chụp, biểu đồ.
- **`third_party/`** — Chỉ dùng nếu bạn vendor thư viện (vd nhúng nguồn Crypto++). Nếu dùng **vcpkg** thì thường để trống.
- **`scripts/`** — Tự động hóa build/benchmark/vẽ chart để **kết quả lặp lại được** (reproducible) — chấm điểm "performance methodology" rất thích điều này.
- **`labN_*/`** — Mỗi lab độc lập: `src/include/tests/kat/benchmark` (+ `resources/`, `certificates/` tùy lab).
- **`build/`** — Thư mục build tạm, **không commit** (thêm `build/` vào `.gitignore`).

---

## 5. Lộ trình làm cả môn (đề xuất)

```
Tuần 1: Setup môi trường (00_windows_setup) + common/ (CLI, codec, file I/O, KAT runner, benchmark)
Tuần 2: Lab 1 (Crypto++ AES) ── nền tảng AEAD/IV, dùng lại cho Lab 3
Tuần 3: Lab 2 (AES manual) ── hiểu sâu nội tại AES; cross-validate với Lab 1
Tuần 4: Lab 3 (RSA + hybrid) ── tái dùng AES-GCM
Tuần 5: Lab 4 (Hash + PKI + attacks) ── demo offline
Tuần 6: Lab 5 (signatures) ── chuẩn bị số liệu để Lab 6 so sánh
Tuần 7: Lab 6 (PQC) ── so sánh với Lab 5
Tuần 8: Gom báo cáo (1 file ~60 trang) + binaries + self-grade + nén nộp
```

> 💡 **Mẹo lớn:** Viết `common/` thật chắc ở Tuần 1. Khoảng 60–70% "Security hygiene", "UX & I/O design", "Performance methodology" của **cả 6 lab** đến từ phần dùng chung này.

---

## 6. Quy ước chung (naming, encoding, git)

- **Tên tool:** mỗi lab có tên riêng theo đề: `aestool` (Lab 1, 2), `rsatool` (Lab 3), `hashtool` (Lab 4), `sigtool` (Lab 5), `pqtool` (Lab 6).
- **Encoding:** mặc định hex cho hiển thị; raw cho file. Luôn hỗ trợ `--encode hex|base64|raw`.
- **Hex:** lowercase, không khoảng trắng. **Base64:** chuẩn RFC 4648.
- **Git commit:** commit nhỏ theo bước (xem "Implementation Roadmap" mỗi lab). Message rõ ràng: `lab1: implement GCM encrypt/decrypt + tag verify`.
- **Encoding file nguồn:** UTF-8 (no BOM) để build được trên cả MSVC/GCC.
- **C++ standard:** C++17 (`set(CMAKE_CXX_STANDARD 17)` + `CMAKE_CXX_STANDARD_REQUIRED ON`).

---

## 7. Bảng rubric tổng quát (Appendix B của đề — tự chấm)

| Criterion | Points | Self-score | TA score |
|-----------|--------|-----------|----------|
| Correctness & KATs | 35–40 | | |
| Security hygiene / misuse checks | 10–20 | | |
| Cross-platform build & UX | 10 | | |
| Performance study quality | 10–20 | | |
| Report quality & clarity | 5–10 | | |
| Negative tests | 10–15 | | |
| Bonuses (if any) | + up to 15 | | |
| **Total** | **100 (+bonus)** | | |

> Mỗi lab có **rubric riêng** (xem mục 1 & 10 trong từng file `labN_*.md`). Bảng trên là khung tổng để **tự chấm** trước khi nộp.

---

## 8. Đọc tiếp

- [00_windows_setup.md](00_windows_setup.md) — Dựng môi trường Windows 11 (VS2022, CMake, vcpkg, OpenSSL, Crypto++).
- [00_cmake_template.md](00_cmake_template.md) — Mẫu CMake C++17 cross-platform + scripts.
- [00_report_template.md](00_report_template.md) — Mẫu báo cáo chung + performance protocol.
- Sau đó vào từng lab: [Lab 1](lab1_aes_cryptopp.md) → [Lab 6](lab6_post_quantum.md).
