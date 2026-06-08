# 📘 Bộ hướng dẫn Cryptography Labs 1–6

> Hướng dẫn chi tiết (tiếng Việt) để hoàn thành chuỗi 6 lab Cryptography trên **Windows 11** (CMake + C++17, build được cả Linux), bám sát rubric trong `Cryptography_Labs.pdf`. Mục tiêu: **đạt tối đa điểm** + báo cáo chuyên nghiệp.

---

## Thứ tự đọc đề xuất

### Bước 1 — Kiến thức nền (đọc trước, dùng cho mọi lab)
1. [00_master_overview.md](00_master_overview.md) — Tổng quan toàn môn, **Common Engineering Requirements**, cấu trúc repository, lộ trình, quy ước.
2. [00_windows_setup.md](00_windows_setup.md) — Dựng môi trường Win11: VS2022, CMake, vcpkg, Crypto++, OpenSSL, MinGW64, verify PATH, lệnh build/run/test/debug.
3. [00_cmake_template.md](00_cmake_template.md) — Mẫu `CMakeLists.txt` C++17 cross-platform, `CMakePresets.json`, `vcpkg.json`, scripts build/benchmark/plot.
4. [00_report_template.md](00_report_template.md) — Mẫu **1 báo cáo chung**, 11 mục/lab, Performance Protocol, checklist ảnh/bảng/biểu đồ, Appendix A (integrity) & B (rubric).

### Bước 2 — Làm từng lab (mỗi file = 12 mục theo OUTPUT FORMAT)
| Lab | Hướng dẫn | Tool | Thư viện | Rubric |
|-----|-----------|------|----------|--------|
| 1 | [lab1_aes_cryptopp.md](lab1_aes_cryptopp.md) | `aestool` | Crypto++ | 100 (+5) |
| 2 | [lab2_aes_manual.md](lab2_aes_manual.md) | `aestool` (tự code) | Không dùng crypto lib | 100 (+20) |
| 3 | [lab3_rsa_hybrid.md](lab3_rsa_hybrid.md) | `rsatool` | Crypto++/OpenSSL | 100 (+15) |
| 4 | [lab4_hash_pki.md](lab4_hash_pki.md) | `hashtool` | OpenSSL/Crypto++ | 100 (+15) |
| 5 | [lab5_signatures.md](lab5_signatures.md) | `sigtool` | Crypto++ + OpenSSL | 100 (+15) |
| 6 | [lab6_post_quantum.md](lab6_post_quantum.md) | `pqtool` | OpenSSL 3.5+ / liboqs | 100 (+15) |

---

## 12 mục trong mỗi file lab
1. Lab Overview · 2. Requirement Breakdown · 3. Folder Structure · 4. Architecture · 5. Classes · 6. Implementation Roadmap · 7. Testing Plan · 8. Benchmark Plan · 9. Security Analysis · 10. Report Template · 11. Common Mistakes · 12. Final Submission Checklist.

---

## ⚠️ Ghi nhớ nhanh (rút từ Common Requirements)
- **CMake out-of-source**, build trên **Ubuntu + Windows (MSVC & MinGW64)**, không sửa path.
- **CLI thống nhất** mọi tool; nhận/xuất UTF-8; **fail closed**.
- RNG: **chỉ** `AutoSeededRandomPool`/`RAND_bytes` — **cấm** `rand()`/`std::random`.
- Key size: AES 256 (báo cáo), RSA ≥ 3072, ECDSA ≥ P-256.
- **KAT** (NIST) + `--kat` JSON runner; **Negative tests**; **Catch2/GoogleTest** + `ctest`.
- Benchmark: warm-up + **N≥30** + mean/median/stddev/**95% CI** + plot có error bar.
- Mỗi report: threat model, misuse, known attacks, secure defaults, fail-closed, limitations.
- Nộp **1 file** `MSV.All6labs.rar` + **1 báo cáo** (~60 trang) + binaries Win/Linux + self-grade + integrity statement.

---

## Tài liệu tham khảo chuẩn
- **FIPS-197** (AES) · **NIST SP 800-38A** (modes) · **SP 800-38D** (GCM)
- **RFC 8017** (PKCS#1 / RSA-OAEP / RSA-PSS) · **RFC 6979** (deterministic ECDSA)
- **FIPS 203** (ML-KEM) · **FIPS 204** (ML-DSA)
- NIST **CAVP/ACVP** test vectors

> Đề gốc: [`../../Cryptography_Labs.pdf`](../../Cryptography_Labs.pdf) (đặt bản PDF tại `docs/specs/` nếu muốn gọn).
