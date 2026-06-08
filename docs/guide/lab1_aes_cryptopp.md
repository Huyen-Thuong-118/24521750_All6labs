# Lab 1 — Symmetric Encryption với Crypto++

> **Tool:** `aestool` · **Thư viện:** CHỈ Crypto++ (cấm OpenSSL/libsodium) · **Rubric:** 100 (+5 bonus)
> Đọc kèm: [Common Requirements](00_master_overview.md#3-common-engineering-requirements-áp-dụng-cho-mọi-lab) · [CMake](00_cmake_template.md) · [Report](00_report_template.md)

---

## 1. Lab Overview

Xây dựng một CLI tool **`aestool`** mã hóa/giải mã đối xứng bằng AES với **8 chế độ runtime-selectable**: `ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM`. Đây là lab nền: nó dạy bạn cách **dùng thư viện crypto an toàn** — phần khó không phải gọi AES, mà là **IV/nonce lifecycle, AEAD, và chống misuse**.

**Ràng buộc (Constraint):**
- Bắt buộc dùng **Crypto++** cho mọi primitive. **Cấm** OpenSSL/libsodium/thư viện crypto khác. STL được phép.
- Compile trên Windows **và** Linux. CMake bắt buộc.

**Learning Outcomes (đề):** dùng Crypto++ qua nhiều mode; xử lý IV/nonce + chống misuse; thiết kế I/O & encoding; validate bằng NIST vectors; benchmark; phát hiện misuse (nonce reuse).

> 🔗 AES-GCM ở lab này sẽ được **tái sử dụng** ở Lab 3 (hybrid). Viết `AESService` sạch để dùng lại.

---

## 2. Requirement Breakdown (Checklist)

### A. Chức năng (functional)
**Bắt buộc:**
- [ ] 8 mode chọn qua `--mode ecb|cbc|ofb|cfb|ctr|xts|ccm|gcm`
- [ ] `encrypt` / `decrypt` commands
- [ ] Key: `--key-hex HEX` **và** `--key KEYFILE` (raw binary hoặc hex có header)
- [ ] IV/nonce: `--iv IVFILE` / `--nonce NONCEFILE`; auto-gen nếu thiếu (trừ ECB/XTS); persist vào header/sidecar
- [ ] AEAD (CCM/GCM): `--aead` + `--aad FILE` / `--aad-text STRING`; verify tag khi decrypt
- [ ] Input `--in` / `--text`; output `--out`; encoding `--encode hex|base64|raw`
- [ ] Sidecar JSON header: `alg, mode, iv, aad, tag`

**Nên làm:** `--verbose`, `--threads N`, key file có header tự mô tả.
**Bonus (+5):** export core thành `.dll/.so/.lib/.a` + GUI Python/C#.

### B. Bảo mật (security hygiene)
- [ ] RNG: **chỉ** `AutoSeededRandomPool` (cấm `rand()`/`std::random`)
- [ ] ECB: in **WARNING**, chặn file > 16 KiB, chỉ cho qua với `--allow-ecb`
- [ ] Enforce đúng IV length theo mode; reject sai length
- [ ] Nonce-reuse protection cho CTR/CCM/GCM: nếu header báo **cùng key+nonce đã dùng → từ chối**
- [ ] AEAD tag fail → **fail closed** (không xuất plaintext)

### C. Testing
- [ ] KAT: NIST SP 800-38A (CBC/CFB/OFB/CTR) + NIST GCM + NIST CCM, runner `--kat vectors.json` (PASS/FAIL + summary)
- [ ] Negative: wrong key, wrong IV, tampered ct (non-AEAD), tampered ct (AEAD)→auth fail, invalid tag→refuse, invalid IV length→reject
- [ ] Unit test (Catch2/GoogleTest), `ctest` pass Win+Linux

### D. Benchmark
- [ ] Sizes: 1KB, 4KB, 16KB, 256KB, 1MB, 8MB · Metrics: throughput MB/s + latency/op
- [ ] So sánh: Windows vs Linux · stream vs block · AEAD vs non-AEAD · tag overhead (GCM/CCM)

### E. Báo cáo
- [ ] Mode-level security (ECB/CBC/CTR/AEAD/GCM/XTS) + impl-level + cross-platform

---

## 3. Folder Structure

```
lab1_aes_cryptopp/
├── CMakeLists.txt
├── include/
│   ├── aes_service.hpp      # mã hóa/giải mã theo mode
│   ├── key_manager.hpp      # nạp/parse/validate key
│   ├── nonce_manager.hpp    # sinh/validate IV + nonce-reuse detection
│   └── sidecar.hpp          # đọc/ghi JSON header
├── src/
│   ├── main.cpp             # CLI: parse args → gọi service
│   ├── aes_service.cpp
│   ├── key_manager.cpp
│   ├── nonce_manager.cpp
│   └── sidecar.cpp
├── tests/
│   ├── test_modes.cpp       # round-trip mọi mode
│   ├── test_aead.cpp        # GCM/CCM tag verify
│   ├── test_negative.cpp
│   └── test_kat.cpp
├── kat/
│   ├── sp800-38a.json       # CBC/CFB/OFB/CTR vectors (NIST)
│   ├── gcm.json             # NIST GCM (gcmEncryptExtIV...)
│   └── ccm.json             # NIST CCM
├── benchmark/
│   └── bench_main.cpp       # → CSV
└── resources/
    └── sample.txt
```

---

## 4. Architecture

```
                 ┌──────────────────────────────┐
   CLI args ───► │  main.cpp (CLI front-end)    │
                 └──────────────┬───────────────┘
                                │ parsed options
                 ┌──────────────▼───────────────┐
                 │   ArgumentParser (common/)   │  --mode --key --iv --aead --encode...
                 └──────────────┬───────────────┘
                                │
        ┌───────────────────────┼───────────────────────┐
        ▼                       ▼                        ▼
┌───────────────┐      ┌─────────────────┐      ┌─────────────────┐
│  KeyManager   │      │  NonceManager   │      │   AESService    │
│ load/validate │      │ gen/validate IV │      │ encrypt/decrypt │
│ key (hex/file)│      │ nonce-reuse log │      │ per-mode + AEAD │
└───────────────┘      └─────────────────┘      └────────┬────────┘
                                                          │ ciphertext + tag
                 ┌────────────────────────────────────────▼──────────┐
                 │ Encoding Layer (hex/base64/raw)  +  Sidecar (JSON) │
                 └────────────────────────────────────────┬──────────┘
                                                           ▼
                                                    File Layer (--out)
```

**Trách nhiệm từng tầng:**
- **CLI / ArgumentParser:** đọc cờ, validate tổ hợp hợp lệ (vd `--aead` chỉ với ccm/gcm), fail-closed khi sai.
- **KeyManager:** nạp key từ hex/file, kiểm tra độ dài 16/24/32 byte.
- **NonceManager:** sinh IV an toàn (AutoSeededRandomPool), enforce length, ghi/đọc log "(keyId,nonce)" để phát hiện reuse.
- **AESService:** ánh xạ mode → object Crypto++ tương ứng, thực thi encrypt/decrypt + tag (AEAD).
- **Encoding/Sidecar/File:** mã hóa hiển thị + ghi metadata + I/O binary-safe.

---

## 5. Classes

| Class | Chức năng | Public methods | Private members |
|-------|-----------|----------------|-----------------|
| `AESService` | Mã hóa/giải mã theo mode | `encrypt(mode, key, iv, pt, aad) → {ct, tag}`; `decrypt(mode, key, iv, ct, tag, aad) → pt` | `dispatchMode()`, helper cho AEAD |
| `KeyManager` | Nạp & validate key | `fromHex(str)`, `fromFile(path)`, `bytes()`, `validateLength(mode)` | `std::vector<byte> key_` |
| `NonceManager` | Sinh/validate IV + chống reuse | `generate(mode)`, `validateLength(mode,iv)`, `checkReuse(keyId,nonce)`, `record(keyId,nonce)` | `AutoSeededRandomPool rng_`, `usedSet_` (file-backed) |
| `Sidecar` | JSON header | `write(path, meta)`, `read(path) → Meta` | nlohmann::json |
| `KatRunner` (common) | Chạy NIST vectors | `run(jsonPath) → Report` | parse + so khớp |
| `BenchmarkRunner` (common) | Đo hiệu năng | `measure(fn, sizes) → CSV` | warm-up + N runs + stats |
| `ReportExporter` (common) | Xuất CSV/summary | `toCsv(results)` | — |

**Map mode → Crypto++ (gợi ý):**
| Mode | Crypto++ |
|------|----------|
| ECB | `ECB_Mode<AES>::Encryption` |
| CBC | `CBC_Mode<AES>` (IV 16B) |
| CFB | `CFB_Mode<AES>` |
| OFB | `OFB_Mode<AES>` |
| CTR | `CTR_Mode<AES>` |
| XTS | `XTS<AES>` (key = 2×, no integrity) |
| CCM | `CCM<AES, TAG_LEN>` (cần set length trước) |
| GCM | `GCM<AES>` (IV 12B khuyến nghị) |

---

## 6. Implementation Roadmap (commit theo bước)

| Step | Việc làm | "Done" khi |
|------|----------|-----------|
| 1 | Setup `lab1/` + CMake link `cryptopp::cryptopp` + skeleton `main` | build ra `aestool` chạy `--help` |
| 2 | CLI parser (dùng `common/`): map mọi cờ | `aestool encrypt --mode cbc ...` parse đúng, sai cờ → fail-closed |
| 3 | `KeyManager` + `NonceManager` (gen/validate, chưa reuse) | nạp key hex/file, sai length → reject |
| 4 | `AESService` non-AEAD: ECB/CBC/CFB/OFB/CTR/XTS round-trip | encrypt→decrypt khôi phục đúng plaintext |
| 5 | AEAD: GCM rồi CCM (+`--aad`), verify tag | tag sai → ném lỗi, không xuất pt |
| 6 | Misuse: ECB warning+16KiB+`--allow-ecb`; nonce-reuse detection | reuse key+nonce → từ chối |
| 7 | Sidecar JSON + encoding hex/base64/raw | header chứa alg/mode/iv/aad/tag |
| 8 | KAT runner + nạp vectors SP800-38A/GCM/CCM | `--kat` in PASS/FAIL + summary, all PASS |
| 9 | Negative tests + unit test (Catch2) | `ctest` pass |
| 10 | Benchmark → CSV + chart | có bảng + plot 6 size |
| 11 | Viết chương Lab 1 trong report | đủ 11 mục |

---

## 7. Testing Plan

### Unit / Integration
| Test | Mục tiêu | Input | Expected |
|------|----------|-------|----------|
| Round-trip mỗi mode | encrypt∘decrypt = identity | key+iv cố định, pt ngẫu nhiên | pt khôi phục đúng |
| AEAD AAD binding | AAD tham gia xác thực | đổi AAD khi decrypt | tag verify **fail** |
| IV persist | đọc lại IV từ sidecar giải mã được | encrypt không truyền iv → auto-gen | decrypt dùng sidecar OK |

### Negative (đề liệt kê rõ — phải có đủ)
| Ca | Input | Expected |
|----|-------|----------|
| Wrong key | decrypt sai key | plaintext sai (non-AEAD) / **refuse** (AEAD) |
| Wrong IV | decrypt sai IV | plaintext sai |
| Tampered ct (non-AEAD) | lật 1 bit ct | output hỏng (không crash) |
| Tampered ct (AEAD) | lật 1 bit ct GCM | **authentication failure**, fail closed |
| Invalid tag | sửa tag | **decryption refusal** |
| Invalid IV length | iv 8 byte cho CBC | **reject** ngay |

### KAT
- **Nguồn:** NIST **SP 800-38A** (CBC/CFB/OFB/CTR), NIST **GCM** test vectors, NIST **CCM** test vectors.
- Định dạng JSON ví dụ:
```json
{
  "algorithm": "AES-128-CBC",
  "tests": [
    {"key":"2b7e151628aed2a6abf7158809cf4f3c","iv":"000102...0f",
     "plaintext":"6bc1bee2...","ciphertext":"7649abac..."}
  ]
}
```
- Runner: parse → encrypt → so khớp ciphertext → in `PASS/FAIL` + `Passed X/Y`.

### Performance
- Đo encrypt+decrypt mỗi mode trên 6 size; mỗi case N≥30 (xem [Performance Protocol](00_report_template.md#3-performance-protocol-copy-nguyên-văn-vào-mỗi-lab)).

---

## 8. Benchmark Plan

- **Framework:** `BenchmarkRunner` (common) — warm-up 1–2s, 1000 ops/block, lặp N≥30, tính mean/median/stddev/95% CI.
- **Sizes:** 1KB, 4KB, 16KB, 256KB, 1MB, 8MB.
- **CSV:**
```csv
algo,mode,os,size_bytes,latency_ms_mean,latency_ms_median,latency_ms_stddev,ci95_ms,throughput_mb_s
AES-256,GCM,Windows,1048576,0.83,0.81,0.05,0.018,1262.7
```
- **So sánh bắt buộc:** Win vs Linux; stream (CFB/OFB/CTR) vs block (ECB/CBC); AEAD (GCM/CCM) vs non-AEAD; tag overhead.
- **Nhận xét mẫu:** "GCM throughput cao nhờ AES-NI + GHASH song song; CCM chậm hơn do 2-pass (MAC-then-encrypt nội bộ)."

---

## 9. Security Analysis

**Threat model:** kẻ tấn công đọc/sửa ciphertext + sidecar (storage/transit), không có key. Mục tiêu: chống đọc trộm & phát hiện sửa đổi (với AEAD).

**Attack surface:** lựa chọn mode, IV/nonce handling, parsing sidecar, RNG, xử lý lỗi.

**Mode-level (đề bắt buộc bàn):**
- **ECB không an toàn:** block giống nhau → ciphertext giống nhau (rò rỉ pattern — ví dụ "ECB penguin").
- **CBC padding oracle:** lỗi padding khác lỗi MAC → oracle giải mã; CBC không có integrity.
- **CTR nonce reuse = thảm họa:** cùng key+nonce → cùng keystream → `C1 ⊕ C2 = P1 ⊕ P2` (two-time pad).
- **AEAD vs non-AEAD:** GCM/CCM cho **confidentiality + integrity + AAD**; ECB/CBC/CTR chỉ confidentiality.
- **GCM cần nonce duy nhất:** reuse → lộ authentication key (forgeries) + two-time pad.
- **XTS không có integrity:** chỉ chống đọc trộm trên disk; sửa block không bị phát hiện.

**Implementation-level:** RNG an toàn (AutoSeededRandomPool), lưu IV an toàn (sidecar), **verify tag trước khi xuất pt**, fail-closed, rủi ro lưu key (plaintext trên disk).

**Cross-platform:** khác biệt RNG giữa OS, hành vi filesystem (newline/flush), biến thiên hiệu năng (scheduler, AES-NI).

**Limitations:** key lưu dạng raw test-key; chưa có KDF/passphrase; nonce-reuse detection chỉ cục bộ (file log) — không chống reuse xuyên máy.

---

## 10. Report Template (mục Lab 1 trong báo cáo)

Theo [mẫu 11 mục](00_report_template.md#2-mẫu-11-mục-cho-mỗi-lab-lặp-lại-cho-lab-16). Riêng Lab 1 cần:
- **KAT Validation:** ảnh `--kat` cho SP800-38A + GCM + CCM; bảng coverage (mode × #case × PASS).
- **Negative Testing:** ảnh GCM tag-fail "Authentication failed — refusing to output".
- **Performance:** bảng 6 size × 8 mode; biểu đồ throughput vs size (error bar) + Win vs Linux + AEAD vs non-AEAD.
- **Security Analysis:** đủ 6 mục mode-level ở trên.
- **Hình cần chụp:** ECB warning + chặn >16KiB; sidecar JSON mẫu; nonce-reuse bị từ chối.

---

## 11. Common Mistakes (lỗi hay gặp)

- ❌ Dùng `rand()`/`std::mt19937` sinh IV → **mất điểm security**. Phải `AutoSeededRandomPool`.
- ❌ GCM dùng IV 16 byte tùy tiện → nên **12 byte (96-bit)** chuẩn; sai length không reject.
- ❌ Xuất plaintext **trước khi** verify tag (AEAD) → vi phạm fail-closed.
- ❌ Quên persist IV → không giải mã lại được.
- ❌ CCM quên `SpecifyDataLengths()` (Crypto++ CCM bắt buộc khai báo length trước).
- ❌ ECB không cảnh báo / không chặn 16KiB / không có `--allow-ecb`.
- ❌ KAT so sánh chuỗi hex có hoa/thường lẫn lộn → chuẩn hóa lowercase.
- ❌ Lỡ `#include <openssl/...>` — **cấm**; Lab 1 chỉ Crypto++.
- ❌ Hardcode path Windows (`\`) → fail trên Linux; dùng `std::filesystem`.

---

## 12. Final Submission Checklist (Lab 1)

- [ ] 8 mode chạy đúng round-trip; AEAD verify tag
- [ ] ECB: warning + 16KiB block + `--allow-ecb`
- [ ] IV auto-gen + persist sidecar; reject sai length
- [ ] Nonce-reuse detection CTR/CCM/GCM
- [ ] `--kat` PASS toàn bộ SP800-38A + GCM + CCM
- [ ] Negative tests đủ 6 ca, fail closed
- [ ] `ctest` pass Windows + Linux
- [ ] Benchmark 6 size + biểu đồ + so sánh Win/Linux & AEAD/non-AEAD
- [ ] Chỉ dùng Crypto++ (grep không có `openssl`)
- [ ] Chương report Lab 1 đủ 11 mục
- [ ] (Bonus) `.dll/.so` + GUI gọi core

**Rubric Lab 1 (100 + 5):** Correctness & KATs **25** · Security hygiene **15** · UX & I/O **10** · Performance methodology **20** · Cross-platform build **10** · Report quality **20** · Bonus library+GUI **+5**.

> Tiếp theo → [Lab 2 — AES manual](lab2_aes_manual.md).
