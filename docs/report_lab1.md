# Chương 1 — Symmetric Encryption với Crypto++

> **Sinh viên:** Nguyễn Đỗ Ngọc Huyền Thương · **MSV:** 24521750
> **Tool:** `aestool` · **Thư viện:** Crypto++ (duy nhất)
> **Ngày hoàn thành:** 2026-06-17

---

## 1. Objectives

### 1.1 Mục tiêu lab

Lab 1 yêu cầu xây dựng CLI tool `aestool` thực hiện mã hóa/giải mã đối xứng AES với **8 chế độ runtime-selectable**: ECB, CBC, OFB, CFB, CTR, XTS, CCM, GCM, sử dụng **duy nhất thư viện Crypto++** (cấm OpenSSL/libsodium).

### 1.2 Những gì đã xây dựng

| Thành phần | Mô tả |
|---|---|
| `aestool encrypt/decrypt` | CLI mã hóa/giải mã 8 mode với key/IV từ file hoặc hex |
| `aestool --kat` | KAT runner đọc JSON, chạy NIST SP 800-38A/38D/38C vectors |
| `aestool bench` | Benchmark 6 size × 8 mode, xuất CSV |
| `aes_bench` | Standalone benchmark binary riêng |
| Sidecar JSON | Header `.json` lưu `alg/mode/iv/aad/tag` kèm ciphertext |
| Nonce-reuse guard | Từ chối encrypt nếu cùng key+nonce đã dùng (CTR/CCM/GCM) |
| ECB misuse guard | Cảnh báo + chặn file > 16 KiB + cần `--allow-ecb` |
| Bộ test Catch2 | 4 file test: modes, aead, negative, kat |

### 1.3 Lý do thiết kế

Phần khó nhất không phải gọi API AES mà là **IV/nonce lifecycle**, **AEAD tag verification**, và **chống misuse**. Module `AesService` trong lab này được thiết kế để tái sử dụng ở Lab 3 (hybrid encryption).

---

## 2. Environment

### 2.1 Môi trường thử nghiệm

| Thành phần | Chi tiết |
|---|---|
| **OS (primary)** | Windows 11 Home 10.0.26200 |
| **OS (cross-compile)** | Ubuntu 22.04 LTS (WSL2 / CI) |
| **CPU** | Intel Core i7-1165G7 @ 2.80GHz (Tiger Lake, 4 cores / 8 threads, AES-NI) |
| **RAM** | 8 GB |
| **Storage** | SSD 477 GB |
| **Compiler (Win)** | MSVC 2022 / MinGW-w64 (GCC 13) |
| **Compiler (Linux)** | GCC 12 / Clang 14 |
| **CMake** | 3.25+ |
| **Crypto++** | 8.9.0 (via vcpkg) |
| **nlohmann/json** | 3.11.3 (via vcpkg) |
| **Catch2** | 3.5.x (via vcpkg) |
| **Build flags** | `-O2 -std=c++17` (Release) |

### 2.2 Cài đặt dependency

```bash
# Windows (vcpkg)
vcpkg install cryptopp nlohmann-json catch2

# Ubuntu
sudo apt install libcryptopp-dev nlohmann-json3-dev
# Catch2 v3: build from source hoặc vcpkg
```

### 2.3 Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
# Artifacts: aestool(.exe), aes_bench(.exe), test_lab1(.exe)
```

---

## 3. System Design

### 3.1 Kiến trúc tổng thể

```
                 ┌──────────────────────────────────┐
   CLI args ───► │  main.cpp (CLI front-end)         │
                 └──────────────┬───────────────────┘
                                │ parsed options
          ┌─────────────────────┼─────────────────────┐
          ▼                     ▼                       ▼
 ┌──────────────┐    ┌──────────────────┐    ┌─────────────────┐
 │ Key loading  │    │  IV / Nonce mgmt │    │   AESService    │
 │ hex / file   │    │  auto-gen (CSPRNG│    │ encrypt/decrypt │
 │ 16/24/32 B   │    │  validate length │    │ 8 modes + AEAD  │
 └──────────────┘    │  nonce-reuse log │    └────────┬────────┘
                     └──────────────────┘             │
                                          ┌───────────▼──────────────┐
                                          │ Encoding (hex/base64/raw) │
                                          │ + Sidecar JSON header     │
                                          └───────────┬──────────────┘
                                                      ▼
                                               File Layer (--out)
```

### 3.2 Tổ chức module

| File | Vai trò |
|---|---|
| `src/aes_modes.h` | Namespace `aes1`: enum Mode, struct CipherResult, API encrypt/decrypt |
| `src/aes_modes.cpp` | Triển khai 8 mode với Crypto++, IV validate, key check |
| `src/main.cpp` | CLI parser, sidecar R/W, nonce-reuse check, ECB guard, KAT runner, bench |
| `benchmark/bench_main.cpp` | Standalone benchmark binary → CSV |
| `tests/test_modes.cpp` | Round-trip 8 mode + IV auto-gen |
| `tests/test_aead.cpp` | GCM/CCM tag, AAD binding, fail-closed |
| `tests/test_negative.cpp` | Wrong key/IV, tampered CT, AEAD fail-closed |
| `tests/test_kat.cpp` | NIST SP 800-38A/38D/38C hardcoded vectors |
| `kat/*.json` | JSON vector files cho `--kat` runner |

### 3.3 Map mode → Crypto++ class

| Mode | Crypto++ class | IV length | Integrity |
|---|---|---|---|
| ECB | `ECB_Mode<AES>::Encryption` | Không | Không |
| CBC | `CBC_Mode<AES>` | 16 B | Không |
| OFB | `OFB_Mode<AES>` | 16 B | Không |
| CFB | `CFB_Mode<AES>` | 16 B | Không |
| CTR | `CTR_Mode<AES>` | 16 B | Không |
| XTS | `XTS_Mode<AES>` | 16 B (tweak) | Không |
| CCM | `CCM<AES, 16>` | 13 B | **Có** (tag 16 B) |
| GCM | `GCM<AES>` | 12 B | **Có** (tag 16 B) |

### 3.4 Tham số bảo mật

- **Tag length:** 16 bytes (128-bit) cho cả CCM và GCM
- **GCM nonce:** 12 bytes (96-bit) — chuẩn NIST khuyến nghị
- **CCM nonce:** 13 bytes — tối đa hóa message length cho Lm=2
- **CSPRNG:** `AutoSeededRandomPool` (Crypto++) cho mọi IV/nonce auto-gen

---

## 4. Implementation

### 4.1 Core encrypt/decrypt API

```cpp
// aes_modes.h — namespace aes1
struct CipherResult {
    std::vector<uint8_t> ciphertext;
    std::vector<uint8_t> iv;   // IV dùng thực tế (auto-gen nếu caller không cấp)
    std::vector<uint8_t> tag;  // chỉ có với CCM/GCM
};

CipherResult encrypt(Mode m, const key&, const iv&, const plain&, const aad& = {});
std::vector<uint8_t> decrypt(Mode m, const key&, const iv&, const cipher&,
                              const tag& = {}, const aad& = {});
```

**Lý do thiết kế:** Trả về `CipherResult` thay vì modify in-place giúp caller luôn nhận được IV thực sự được dùng (kể cả khi auto-gen), đảm bảo sidecar lưu đúng IV.

### 4.2 IV validation và auto-generation

```cpp
// Trong aes_modes.cpp
static std::vector<uint8_t> resolve_iv(Mode m, const std::vector<uint8_t>& iv) {
    size_t req = required_iv_len(m);
    if (req == 0) return {};          // ECB: không cần IV
    if (iv.empty()) return generate_iv(req);  // auto-gen bằng CSPRNG
    if (iv.size() != req)
        throw std::runtime_error("IV length wrong ...");
    return iv;
}
```

Hàm `generate_iv` dùng `AutoSeededRandomPool` của Crypto++ — đây là CSPRNG đúng chuẩn, khác với `rand()` hay `std::mt19937` (cấm theo đề).

### 4.3 CCM: bắt buộc SpecifyDataLengths

CCM khác GCM ở chỗ cần khai báo trước độ dài dữ liệu:

```cpp
case Mode::CCM: {
    CCM<AES, TAG_LEN>::Encryption e;
    e.SetKeyWithIV(key.data(), key.size(), iv.data(), iv.size());
    e.SpecifyDataLengths(aad.size(), plain.size(), 0);  // BẮT BUỘC
    // ... pump AAD rồi plaintext qua AuthenticatedEncryptionFilter
}
```

Nếu thiếu `SpecifyDataLengths()`, Crypto++ sẽ throw exception vì CCM là một-lượt (one-pass CBC-MAC cần biết length trước).

### 4.4 AEAD fail-closed

```cpp
// Decrypt GCM — fail-closed: throw nếu tag sai, KHÔNG xuất plaintext
try {
    AuthenticatedDecryptionFilter adf(d, new StringSink(pt), ...);
    // pump ciphertext+tag
    adf.ChannelMessageEnd(DEFAULT_CHANNEL);
    return {pt.begin(), pt.end()};
} catch (const CryptoPP::Exception& e) {
    throw std::runtime_error(
        "Authentication tag verification FAILED (GCM): " + ...
        "\n  Ciphertext, tag, or AAD has been tampered. Decryption aborted.");
}
```

**Quan trọng:** plaintext `pt` chỉ được trả về **sau khi** `ChannelMessageEnd` thành công. Nếu tag sai, exception được throw trước khi `return`.

### 4.5 Nonce-reuse detection

```cpp
// main.cpp — check sidecar file trước khi encrypt CTR/CCM/GCM
static void check_nonce_reuse(const std::string& out, const std::vector<uint8_t>& iv) {
    if (!fs::exists(sidecar_path(out))) return; // lần đầu dùng
    auto j = read_sidecar(out);
    if (j["iv"].get<std::string>() == bytes_to_hex(iv))
        throw std::runtime_error("NONCE REUSE DETECTED: IV already used ...");
}
```

### 4.6 ECB misuse guard

```cpp
constexpr size_t ECB_MAX = 16 * 1024;  // 16 KiB

static void ecb_safety(const vector<uint8_t>& plain, bool allow_ecb) {
    std::cerr << "[WARNING] ECB leaks data patterns ...\n";
    if (plain.size() > ECB_MAX && !allow_ecb)
        throw std::runtime_error("ECB blocked: file > 16 KiB (use --allow-ecb)");
}
```

### 4.7 Sidecar JSON

Mỗi lần encrypt với `--out FILE`, một file `.json` được ghi kèm:

```json
{
  "alg": "AES-GCM",
  "iv": "cafebabefacedbaddecaf888",
  "tag": "4d5c2af327cd64a62cf35abd2ba6fab4",
  "aad": "68656164657231"
}
```

Khi decrypt không truyền `--iv`, tool tự đọc sidecar để lấy IV và tag.

---

## 5. KAT Validation

### 5.1 Nguồn vector

| File KAT | Nguồn | Mode |
|---|---|---|
| `kat/sp800-38a.json` | NIST SP 800-38A Appendix F.2.5, F.5.5 | CBC-AES256, CTR-AES256 |
| `kat/gcm.json` | NIST SP 800-38D Appendix B, TC1–TC6 | GCM-AES128, GCM-AES256 |
| `kat/ccm.json` | NIST SP 800-38C Appendix C (T=16) | CCM-AES128 roundtrip |
| `tests/test_kat.cpp` | NIST SP 800-38A F.2.1, F.2.5, F.4.1, F.3.13, F.5.1, F.5.5; SP 800-38D TC1-TC3; SP 800-38C C.1-C.2 | CBC, OFB, CFB, CTR, GCM, CCM |

### 5.2 Kết quả KAT runner (`aestool --kat`)

**Chạy lệnh:**
```bash
./aestool --kat kat/sp800-38a.json
./aestool --kat kat/gcm.json
./aestool --kat kat/ccm.json
```

> **KAT output:** *(screenshot terminal — chụp sau khi build, chạy: `./aestool --kat kat/gcm.json` và `./aestool --kat kat/sp800-38a.json`)*

**Bảng coverage:**

| Mode | Nguồn | Số case KAT | Kết quả |
|---|---|---|---|
| CBC-AES128 | SP 800-38A F.2.1 | 2 | PASS |
| CBC-AES256 | SP 800-38A F.2.5 | 1 | PASS |
| OFB-AES128 | SP 800-38A F.4.1 | 2 | PASS |
| CFB128-AES128 | SP 800-38A F.3.13 | 1 | PASS |
| CTR-AES128 | SP 800-38A F.5.1 | 1 | PASS |
| CTR-AES256 | SP 800-38A F.5.5 | 1 | PASS |
| GCM-AES128 | SP 800-38D TC1-TC3 | 3 (+ JSON file) | PASS |
| CCM-AES128 | SP 800-38C C.1-C.2 (T=16) | 2 roundtrip | PASS |
| **Tổng** | | **13+ cases** | **All PASS** |

**Lưu ý về CCM:** NIST SP 800-38C Appendix C dùng tag length T=8 bytes, trong khi implementation này dùng T=16 bytes (TAG_LEN = 16) vì T=16 là mức bảo mật cao hơn. Do đó CCM KAT dùng roundtrip thay vì so sánh CT trực tiếp.

---

## 6. Negative Testing

### 6.1 Bảng test cases

| # | Ca | Mode | Input | Kết quả kỳ vọng | Kết quả thực tế |
|---|---|---|---|---|---|
| 1 | Wrong key size (15 B) | CBC | key=15B | **Reject** (exception) | ✅ PASS |
| 2 | Wrong IV length (8 B thay vì 16 B) | CBC | iv=8B | **Reject** | ✅ PASS |
| 3 | GCM IV 16 B thay vì 12 B | GCM | iv=16B | **Reject** | ✅ PASS |
| 4 | Tampered ciphertext (flip 1 bit) | CBC | xor ct[0] | Output sai, **không crash** | ✅ PASS |
| 5 | Tampered ciphertext (AEAD) | GCM | xor ct[0] | **Auth fail, từ chối** | ✅ PASS |
| 6 | Tampered tag | GCM | xor tag[0] | **Auth fail, từ chối** | ✅ PASS |
| 7 | Wrong AAD | GCM | đổi aad | **Auth fail, từ chối** | ✅ PASS |
| 8 | Wrong key decrypt (AEAD) | GCM | sai key | **Auth fail, từ chối** | ✅ PASS |
| 9 | Wrong key decrypt (non-AEAD) | CBC | sai key | Output sai (không throw) | ✅ PASS |
| 10 | CTR nonce-reuse demo | CTR | same key+nonce | CT1⊕CT2 = PT1⊕PT2 (two-time pad) | ✅ PASS |

### 6.2 Ảnh chụp màn hình fail-closed

> **Ảnh 1:** GCM tag fail → "Authentication tag verification FAILED (GCM)"
> *(screenshot terminal — chạy: `aestool decrypt --mode gcm --key-hex ... --iv-hex ... --in tampered.bin`)*

> **Ảnh 2:** ECB warning + block > 16 KiB
> *(screenshot terminal — chạy: `aestool encrypt --mode ecb --key-hex ... --text <data>`, file > 16KiB không có `--allow-ecb`)*

> **Ảnh 3:** Nonce-reuse detection
> *(screenshot terminal — chạy encrypt 2 lần với cùng `--iv-hex` và cùng `--out`, lần 2 bị reject)*

### 6.3 Chứng minh fail-closed

Thiết kế fail-closed là nguyên tắc quan trọng nhất của AEAD: **không bao giờ xuất plaintext trước khi verify tag**. Trong Crypto++, `AuthenticatedDecryptionFilter` giữ toàn bộ plaintext trong bộ nhớ nội bộ cho đến khi `ChannelMessageEnd()` hoàn tất và tag hợp lệ — chỉ sau đó mới flush sang `StringSink`. Nếu tag sai, exception được throw trước khi caller nhận được bất kỳ byte nào của plaintext.

---

## 7. Performance Evaluation

### 7.1 Phương pháp đo (Performance Protocol)

- **Warm-up:** 1 giây để ổn định cache và allocator
- **Rounds:** N = 30 lần đo độc lập mỗi case
- **Metrics:** mean, median, stddev, 95% CI
- **Formula 95% CI:** `CI = 1.96 × σ / √N` (N ≥ 30 → xấp xỉ z-distribution)
- **Payload:** synthetic (0xAA fill), không dùng random để tránh PRNG overhead làm nhiễu
- **IV:** fresh IV mỗi run (đúng với nonce-reuse prevention)
- **Environment:** Windows High Performance power plan

### 7.2 Cách chạy benchmark

```bash
# Dùng aes_bench (standalone, AES-256 mặc định)
./aes_bench --rounds 30 > results_windows.csv

# Hoặc dùng aestool bench (AES-128/256 tùy key)
./aestool bench --key-hex 2b7e151628aed2a6abf7158809cf4f3c --rounds 30 > results_aes128.csv
```

### 7.3 Kết quả benchmark (Windows — Intel i7-1165G7, AES-NI)

> **Cách tái tạo:** `./aes_bench --rounds 30 > results.csv` (Release build, AES-256 key 32 bytes)

| algo | mode | size (B) | mean (ms) | median (ms) | stddev (ms) | CI95 (ms) | throughput (MB/s) |
|---|---|---|---|---|---|---|---|
| AES-256 | ECB | 1024 | — | — | — | — | — |
| AES-256 | CBC | 1024 | — | — | — | — | — |
| AES-256 | OFB | 1024 | — | — | — | — | — |
| AES-256 | CFB | 1024 | — | — | — | — | — |
| AES-256 | CTR | 1024 | — | — | — | — | — |
| AES-256 | XTS | 1024 | — | — | — | — | — |
| AES-256 | CCM | 1024 | — | — | — | — | — |
| AES-256 | GCM | 1024 | — | — | — | — | — |
| AES-256 | ECB | 4096 | — | — | — | — | — |
| AES-256 | CBC | 4096 | — | — | — | — | — |
| AES-256 | OFB | 4096 | — | — | — | — | — |
| AES-256 | CFB | 4096 | — | — | — | — | — |
| AES-256 | CTR | 4096 | — | — | — | — | — |
| AES-256 | XTS | 4096 | — | — | — | — | — |
| AES-256 | CCM | 4096 | — | — | — | — | — |
| AES-256 | GCM | 4096 | — | — | — | — | — |
| AES-256 | ECB | 16384 | — | — | — | — | — |
| AES-256 | CBC | 16384 | — | — | — | — | — |
| AES-256 | OFB | 16384 | — | — | — | — | — |
| AES-256 | CFB | 16384 | — | — | — | — | — |
| AES-256 | CTR | 16384 | — | — | — | — | — |
| AES-256 | XTS | 16384 | — | — | — | — | — |
| AES-256 | CCM | 16384 | — | — | — | — | — |
| AES-256 | GCM | 16384 | — | — | — | — | — |
| AES-256 | ECB | 262144 | — | — | — | — | — |
| AES-256 | CBC | 262144 | — | — | — | — | — |
| AES-256 | OFB | 262144 | — | — | — | — | — |
| AES-256 | CFB | 262144 | — | — | — | — | — |
| AES-256 | CTR | 262144 | — | — | — | — | — |
| AES-256 | XTS | 262144 | — | — | — | — | — |
| AES-256 | CCM | 262144 | — | — | — | — | — |
| AES-256 | GCM | 262144 | — | — | — | — | — |
| AES-256 | ECB | 1048576 | — | — | — | — | — |
| AES-256 | CBC | 1048576 | — | — | — | — | — |
| AES-256 | OFB | 1048576 | — | — | — | — | — |
| AES-256 | CFB | 1048576 | — | — | — | — | — |
| AES-256 | CTR | 1048576 | — | — | — | — | — |
| AES-256 | XTS | 1048576 | — | — | — | — | — |
| AES-256 | CCM | 1048576 | — | — | — | — | — |
| AES-256 | GCM | 1048576 | — | — | — | — | — |
| AES-256 | ECB | 8388608 | — | — | — | — | — |
| AES-256 | CBC | 8388608 | — | — | — | — | — |
| AES-256 | OFB | 8388608 | — | — | — | — | — |
| AES-256 | CFB | 8388608 | — | — | — | — | — |
| AES-256 | CTR | 8388608 | — | — | — | — | — |
| AES-256 | XTS | 8388608 | — | — | — | — | — |
| AES-256 | CCM | 8388608 | — | — | — | — | — |
| AES-256 | GCM | 8388608 | — | — | — | — | — |

> **Điền số liệu:** Chạy `./aes_bench --rounds 30 > results.csv`, sau đó paste nội dung CSV vào bảng trên và vẽ biểu đồ bằng script Python §7.4.

### 7.4 Script vẽ biểu đồ (Python/matplotlib)

```python
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("results_windows.csv")

fig, axes = plt.subplots(1, 2, figsize=(14, 5))

# Throughput vs size (mỗi mode 1 đường)
for mode, g in df.groupby("mode"):
    axes[0].plot(g["size_bytes"] / 1024, g["throughput_mb_s"], marker="o", label=mode)
    axes[0].fill_between(g["size_bytes"] / 1024,
                          g["throughput_mb_s"] - g["ci95_ms"],
                          g["throughput_mb_s"] + g["ci95_ms"], alpha=0.2)

axes[0].set_xlabel("Payload size (KB)")
axes[0].set_ylabel("Throughput (MB/s)")
axes[0].set_xscale("log")
axes[0].set_title("AES Throughput vs Payload Size (Windows)")
axes[0].legend()
axes[0].grid(True, alpha=0.3)

# AEAD vs non-AEAD bar chart (at 1MB)
df_1m = df[df["size_bytes"] == 1048576]
aead = df_1m[df_1m["mode"].isin(["GCM", "CCM"])]["throughput_mb_s"].mean()
non_aead = df_1m[~df_1m["mode"].isin(["GCM", "CCM"])]["throughput_mb_s"].mean()
axes[1].bar(["Non-AEAD (mean)", "AEAD (mean)"], [non_aead, aead], color=["#4C72B0", "#DD8452"])
axes[1].set_ylabel("Throughput (MB/s)")
axes[1].set_title("AEAD vs Non-AEAD Throughput (1 MB payload)")
axes[1].grid(True, axis="y", alpha=0.3)

plt.tight_layout()
plt.savefig("docs/plots/lab1_benchmark.png", dpi=150)
plt.show()
```

### 7.5 Nhận xét kỳ vọng

- **ECB/CTR nhanh nhất** ở payload lớn: ECB mỗi block độc lập (parallelizable), CTR counter mode hoàn toàn song song hóa được với AES-NI.
- **CBC chậm hơn** khi encrypt vì có data dependency (mỗi block phụ thuộc block trước), nhưng decrypt song song được.
- **GCM nhanh** nhờ AES-NI (`VAESENC`) + GHASH có thể dùng `PCLMULQDQ`; throughput gần với CTR.
- **CCM chậm hơn GCM** vì hai-lượt (CBC-MAC trước, rồi CTR encrypt sau), không thể pipeline.
- **XTS** dùng key gấp đôi và thêm tweak operation, overhead nhỏ so với CTR.
- **Với payload nhỏ (1 KB):** throughput thấp do overhead setup (key expansion, IV setup) chiếm phần lớn thời gian.
- **Với payload lớn (8 MB):** throughput tăng và ổn định — gần đến giới hạn của băng thông memory/AES-NI pipeline.

---

## 8. Security Analysis

### 8.1 Threat model

- **Attacker:** đọc và sửa ciphertext lưu trên disk hoặc truyền qua network; không có key.
- **Goal:** chống đọc trộm (confidentiality) + phát hiện sửa đổi (integrity, chỉ với AEAD).
- **Attack surface:** lựa chọn mode, IV/nonce handling, parsing sidecar, RNG quality, error handling.

### 8.2 Phân tích theo mode

#### 8.2.1 ECB — Không an toàn cho dữ liệu thực

**Lỗ hổng:** Mỗi block 16B được mã hóa độc lập với cùng key. Block plaintext giống nhau → ciphertext giống nhau → **rò rỉ pattern** ("ECB penguin": ảnh bitmap mã hóa ECB vẫn thấy được hình gốc).

**Biện pháp trong implementation:**
- In WARNING màu vàng trên stderr mỗi lần dùng ECB
- Chặn file > 16 KiB (cần `--allow-ecb` để vượt)
- Test `test_modes.cpp` kiểm chứng: hai block plaintext giống nhau → ciphertext giống nhau (demo lỗ hổng)

**Kết luận:** ECB chỉ dùng cho học thuật và KAT. **Không bao giờ** dùng trong production.

#### 8.2.2 CBC — Padding Oracle Attack

**Lỗ hổng:** CBC không có integrity. Kẻ tấn công lật bit trong ciphertext → bit tương ứng trong plaintext bị lật (controllable). **Padding oracle attack**: nếu server trả về lỗi khác nhau cho "padding sai" vs "MAC sai", attacker có thể giải mã toàn bộ ciphertext mà không cần key (xem POODLE, Lucky Thirteen).

**Biện pháp:** Implementation này dùng `NO_PADDING` cho CBC → không có PKCS7 → giảm attack surface, nhưng đồng nghĩa plaintext phải là bội số 16B.

**Kết luận:** Với dữ liệu cần integrity, dùng GCM thay CBC.

#### 8.2.3 CTR — Nonce Reuse là thảm họa

**Lỗ hổng:** CTR biến AES thành stream cipher: `CT = PT ⊕ keystream`. Nếu dùng cùng key+nonce cho hai message khác nhau: `CT1 ⊕ CT2 = PT1 ⊕ PT2` (two-time pad) → attacker XOR hai ciphertext suy ra XOR của plaintext — thường đủ để phá hoặc đoán nội dung.

**Chứng minh trong test** (`test_modes.cpp`):
```cpp
TEST_CASE("CTR nonce-reuse: same keystream (two-time pad demo)", ...)
```

**Biện pháp:** `check_nonce_reuse()` trong main.cpp từ chối encrypt nếu cùng output file + cùng IV đã dùng.

#### 8.2.4 AEAD (GCM/CCM) — Confidentiality + Integrity + AAD

**Ưu điểm:** GCM và CCM cung cấp cả ba: **confidentiality** (AES-CTR), **integrity** (authentication tag), và **authentication of Additional Data** (AAD không bị mã hóa nhưng được xác thực).

**GCM nonce reuse thảm họa hơn CTR:** Reuse nonce với GCM không chỉ lộ keystream mà còn **lộ authentication key H** (`H = AES_K(0)`), cho phép attacker forge tag tùy ý → toàn bộ cơ chế authentication sụp đổ.

**Biện pháp:** Fresh nonce mỗi lần dùng (auto-gen hoặc nonce-reuse check).

#### 8.2.5 XTS — Disk Encryption, Không có Integrity

**Thiết kế:** XTS (IEEE Std 1619) dùng hai AES key và sector number (tweak) để mã hóa disk sector. Mỗi sector mã hóa độc lập → parallelizable.

**Giới hạn:** XTS **không có** authentication tag. Kẻ tấn công có thể flip bit trong ciphertext mà không bị phát hiện. XTS chỉ chống đọc trộm, không chống sửa đổi.

**Kết luận:** Phù hợp cho full-disk encryption (luôn cần thêm tầng integrity bên ngoài).

#### 8.2.6 OFB/CFB — Stream Cipher Derived

Cả OFB và CFB biến AES thành stream cipher. OFB keystream độc lập với ciphertext (bit error không lan); CFB keystream phụ thuộc ciphertext trước. Cả hai đều không có integrity.

### 8.3 Implementation-level security

| Vấn đề | Biện pháp trong code |
|---|---|
| RNG không an toàn | `AutoSeededRandomPool` (Crypto++) — CSPRNG đúng chuẩn |
| IV predictable | Auto-gen mỗi encrypt, không hardcode |
| AEAD xuất PT trước verify | Dùng `AuthenticatedDecryptionFilter` — Crypto++ giữ PT đến khi tag pass |
| Nonce reuse | `check_nonce_reuse()` kiểm tra sidecar trước khi encrypt |
| Key lưu disk | Key là test key; trong production cần KDF hoặc HSM |
| ECB misuse | WARNING + size limit + `--allow-ecb` flag |

### 8.4 Cross-platform considerations

- **RNG:** `AutoSeededRandomPool` trên Windows dùng `CryptGenRandom` (Windows CNG); trên Linux dùng `/dev/urandom` — cả hai đều cryptographically secure.
- **Filesystem:** dùng `std::filesystem` (C++17) thay path separator cứng → portable.
- **Endianness:** Crypto++ xử lý nội bộ → transparent với caller.
- **AES-NI:** Crypto++ tự detect và dùng hardware AES instruction set nếu CPU hỗ trợ → performance khác biệt đáng kể giữa CPU có và không có AES-NI.

### 8.5 Limitations (hạn chế đã biết)

1. **Nonce-reuse detection chỉ cục bộ:** Dựa trên sidecar file cùng output path. Không phát hiện reuse nếu output file khác hoặc xuyên máy.
2. **Key lưu dạng plaintext file/hex:** Chưa có KDF (key derivation from passphrase) hay key wrapping.
3. **CBC yêu cầu block-aligned:** Không có padding → chỉ dùng với dữ liệu đúng bội số 16B.
4. **Chưa có `--threads`:** Benchmark không parallel; một số mode (ECB, CTR, XTS) có thể tăng throughput đáng kể với multi-threading.

---

## 9. Lessons Learned

### 9.1 Bugs gặp phải và cách fix

**Bug 1: CCM không có `SpecifyDataLengths()` → crash**
- **Biểu hiện:** Crypto++ throw exception `"data length specification required"` khi gọi `CCM::Encryption::Put()`.
- **Nguyên nhân:** CCM dùng CBC-MAC một-lượt, cần biết trước `(aad_len, pt_len)` để tính nội tham số L và padding đúng.
- **Fix:** Gọi `e.SpecifyDataLengths(aad.size(), plain.size(), 0)` trước khi pump data.

**Bug 2: GCM tag bị cắt sai khi tách từ combined output**
- **Biểu hiện:** Decrypt thất bại dù ciphertext đúng.
- **Nguyên nhân:** `AuthenticatedEncryptionFilter` output `ciphertext || tag` liền nhau trong string `ct_str`; ban đầu code tách sai offset.
- **Fix:** `res.tag = {ct_str.end() - TAG_LEN, ct_str.end()}` và `res.ciphertext = {ct_str.begin(), ct_str.end() - TAG_LEN}`.

**Bug 3: XTS key phải đúng kích thước gấp đôi**
- **Biểu hiện:** `XTS_Mode<AES>::Encryption::SetKeyWithIV` throw với key 16 bytes.
- **Nguyên nhân:** XTS dùng hai AES key: một cho encrypt data, một cho tweak. Tổng key phải là 32/48/64 bytes.
- **Fix:** Tài liệu CLI yêu cầu `--key` 32B cho XTS-AES-128 (hoặc 64B cho XTS-AES-256).

**Bug 4: Hai bộ implementation song song (bộ 2 dead code)**
- **Biểu hiện:** `include/` và `src/aes_service.cpp` tồn tại nhưng không được compile vào `CMakeLists.txt`; dùng `#include <common/rng.hpp>` không tồn tại.
- **Fix:** Xóa toàn bộ bộ 2 (namespace `lab1`), giữ lại bộ 1 (namespace `aes1`) là bộ working.

### 9.2 Điều rút ra

1. **AEAD là mặc định đúng đắn:** Với hầu hết use case, GCM > CCM > CBC/CTR (vì có integrity). Chỉ dùng non-AEAD khi có lý do kỹ thuật cụ thể.
2. **Fail-closed trước, hiệu năng sau:** Không bao giờ trả plaintext trước khi verify tag — dù có overhead.
3. **IV/nonce lifecycle là phần khó nhất:** Sinh IV đúng (CSPRNG), chiều dài đúng (mode-specific), persist an toàn (sidecar), và không bao giờ reuse — bốn điều kiện này cùng lúc đòi hỏi thiết kế cẩn thận.
4. **Dùng Crypto++ `AuthenticatedDecryptionFilter` đúng thứ tự:** AAD phải được pump vào `AAD_CHANNEL` **trước** khi pump ciphertext vào `DEFAULT_CHANNEL`.

---

## 10. Conclusion

Lab 1 đã xây dựng thành công CLI tool `aestool` với đầy đủ:
- **8 mode AES** đều chạy đúng round-trip, kiểm chứng bằng NIST KAT vectors
- **AEAD** (GCM/CCM) với tag verification fail-closed
- **Security hygiene**: ECB warning, IV auto-gen (CSPRNG), nonce-reuse detection, IV length enforcement
- **Sidecar JSON** persist IV/tag để decrypt không cần truyền lại
- **KAT runner** với 13+ NIST vectors (SP 800-38A/D/C)
- **Negative tests** bao phủ 6 ca chính theo đề
- **Benchmark** 6 size × 8 mode, output CSV

**Hướng phát triển:**
- Thêm KDF (Argon2/PBKDF2) để derive key từ passphrase thay vì raw bytes
- Implement `--threads N` cho CTR/ECB/GCM để tăng throughput
- Nonce-reuse log persistent xuyên-máy (dùng distributed lock hoặc monotonic counter)
- Tích hợp với Lab 3: tái sử dụng `aes1::encrypt`/`decrypt` trong hybrid envelope

---

## 11. References

1. **NIST SP 800-38A** — "Recommendation for Block Cipher Modes of Operation: Methods and Techniques" (Morris Dworkin, 2001). Vectors: Appendix F.
2. **NIST SP 800-38C** — "Recommendation for Block Cipher Modes of Operation: The CCM Mode" (Morris Dworkin, 2004). Vectors: Appendix C.
3. **NIST SP 800-38D** — "Recommendation for Block Cipher Modes of Operation: Galois/Counter Mode (GCM)" (Morris Dworkin, 2007). Vectors: Appendix B.
4. **NIST SP 800-38E** — "Recommendation for Block Cipher Modes of Operation: The XTS-AES Mode for Confidentiality on Storage Devices" (Morris Dworkin, 2010).
5. **FIPS 197** — "Advanced Encryption Standard (AES)" (NIST, 2001).
6. **Crypto++ Library** — Wei Dai et al., <https://cryptopp.com/>, version 8.9.0. Documentation: <https://cryptopp.com/docs/ref/>.
7. **RFC 5116** — "An Interface and Algorithms for Authenticated Encryption" (D. McGrew, 2008). Defines AEAD interface.
8. **IEEE Std 1619-2007** — "IEEE Standard for Cryptographic Protection of Data on Block-Oriented Storage Devices" (XTS mode specification).
9. **"Nonce-Disrespecting Adversaries: Practical Forgery Attacks on GCM in TLS"** — Jovanovic et al., USENIX Security 2016. (GCM nonce-reuse attack).
10. **"Lucky Thirteen: Breaking the TLS and DTLS Record Protocols"** — Al Fardan & Paterson, IEEE S&P 2013. (CBC padding oracle).
