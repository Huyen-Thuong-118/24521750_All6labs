# Chương 4 — Hashing, PKI & Practical Attacks

> **Sinh viên:** Nguyễn Đỗ Ngọc Huyền Thương · **MSV:** 24521750
> **Tool:** `hashtool` · **Thư viện:** OpenSSL 3.6.2 (EVP API)
> **Ngày hoàn thành:** 2026-06-22

---

## 1. Objectives

### 1.1 Mục tiêu lab

Lab 4 yêu cầu xây dựng CLI tool `hashtool` với bốn nhóm chức năng chính:

1. **Hashing** — hỗ trợ 10 thuật toán: SHA-224, SHA-256, SHA-384, SHA-512 (SHA-2 family), SHA3-224, SHA3-256, SHA3-384, SHA3-512 (SHA-3 family), SHAKE128, SHAKE256 (XOF).
2. **X.509 Certificate Parsing** — đọc chứng chỉ PEM/DER, trích xuất Subject, Issuer, Validity, SPKI, SAN, KeyUsage, BasicConstraints; verify chữ ký CA.
3. **KAT Validation** — chạy Known-Answer Test từ file JSON với vectors NIST FIPS 180-4 và FIPS 202.
4. **Length-Extension Attack (Bonus +5)** — implement SHA-256 compression từ đầu, forge `SHA256(key‖msg‖pad‖ext)` không cần biết key.

### 1.2 Những gì đã xây dựng

| Thành phần | Mô tả |
|---|---|
| `hashtool hash` | Hash file hoặc stdin, 10 algo, output hex |
| `hashtool cert` | Parse X.509 PEM/DER, in 8+ trường |
| `hashtool kat` | Đọc JSON, so sánh expected vs actual hex |
| `hashtool extend` | Thực hiện SHA-256 length-extension attack |
| `sha256_ext.cpp` | SHA-256 compression function tự viết, padding, forge |
| `cert_parser.cpp` | OpenSSL X509 API wrapper |
| KAT JSON | `sha2.json`, `sha3.json`, `shake.json` — NIST FIPS 180-4/202 |
| `lab4_tests` | 64 Catch2 tests, 120 assertions |
| `lab4_bench` | Benchmark SHA-2/SHA-3 tại 1 MiB và 100 MiB |

### 1.3 Lý do thiết kế

Tất cả hashing đi qua một interface duy nhất (`hash_bytes` / `hash_file`) với enum `Algo`. SHA-3 và SHAKE dùng cùng EVP pipeline — điểm khác biệt duy nhất là SHAKE cần gọi `EVP_DigestFinalXOF` thay vì `EVP_DigestFinal_ex` vì output length biến đổi.

---

## 2. Environment

### 2.1 Môi trường thử nghiệm

| Thành phần | Chi tiết |
|---|---|
| **OS** | Windows 11 Home 10.0.26200 |
| **CPU** | Intel Core i7-1165G7 @ 2.80 GHz (Tiger Lake, 4C/8T, SHA-NI extension) |
| **RAM** | 8 GB DDR4 |
| **Compiler** | MSVC 2022 (cl 19.x), C++17, `/O2` |
| **OpenSSL** | 3.6.2 (via vcpkg) |
| **nlohmann/json** | 3.11.3 (via vcpkg) |
| **Catch2** | 3.5.x (via vcpkg) |
| **CMake** | 3.25+ |

### 2.2 Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target hashtool lab4_tests lab4_bench --config Release
```

---

## 3. System Design

### 3.1 Kiến trúc tổng thể

```
hashtool CLI (main.cpp)
      │
      ├── cmd_hash  ──► hasher.cpp [Algo enum → EVP pipeline]
      │                    ├── SHA-2: EVP_DigestFinal_ex
      │                    ├── SHA-3: EVP_DigestFinal_ex
      │                    └── SHAKE: EVP_DigestFinalXOF (outlen required)
      │
      ├── cmd_cert  ──► cert_parser.cpp [OpenSSL X509 API]
      │                    ├── load_cert (PEM → fallback DER)
      │                    ├── extract: subject/issuer/validity/SPKI/SAN/KeyUsage/IsCA
      │                    └── verify_cert_signature: X509_verify(cert, issuer_pubkey)
      │
      ├── cmd_kat   ──► hasher.cpp + nlohmann/json
      │                    └── reads sha2.json / sha3.json / shake.json
      │
      └── cmd_extend ──► sha256_ext.cpp [SHA-256 từ đầu]
                            ├── sha256_compression(state, block)
                            ├── sha256_padding(total_len)
                            └── extend(hash_h, key_len, msg, extension)
```

### 3.2 Thiết kế module

**`hasher.cpp`:**
- `parse_algo(string)` — từ chối "md5", "sha1" (throw), chấp nhận 10 algo hợp lệ.
- `hash_bytes(Algo, data, outlen)` — XOF cần `outlen > 0`.
- `hash_file(Algo, path, outlen)` — stream 65536-byte chunks, không load hết file vào RAM.

**`cert_parser.cpp`:**
- `load_cert(path)` — thử PEM trước, fallback DER.
- `extract(X509*)` — dùng `X509_get_ext_by_NID(NID_basic_constraints)` + `X509V3_EXT_d2i` để đọc IsCA.
- `verify_cert_signature(cert_path, issuer_path)` — `X509_verify(cert, issuer_pubkey)`.

**`sha256_ext.cpp` (Bonus):**
- 64 round constants K[64], bitwise ops `rotr`, `sigma0/sigma1/Sigma0/Sigma1`.
- `sha256_compression(state[8], block[64])` — 1 compression round.
- `sha256_padding(total_len)` → `0x80 ‖ zeros ‖ 64-bit BE bitlen`.
- `extend(hash_h, key_len, msg, extension)` → phục hồi internal state từ hash_h, tiếp tục hash extension.

---

## 4. Implementation

### 4.1 SHAKE XOF — phân biệt với SHA-3 thông thường

```cpp
// SHA-3 thường
EVP_DigestFinal_ex(ctx, out.data(), &outlen_u);

// SHAKE XOF — PHẢI dùng XOF variant, truyền output length rõ ràng
EVP_DigestFinalXOF(ctx, out.data(), outlen);  // outlen do caller chỉ định
```

Nếu caller không truyền `outlen` cho SHAKE, hàm throw `std::runtime_error` để tránh truncation mặc định.

### 4.2 X.509 IsCA — vấn đề với `X509_check_ca`

`X509_check_ca(cert)` trả về 0 ngay cả khi cert có `BasicConstraints CA:TRUE` nếu extension được add bằng `X509V3_EXT_conf_nid` trả NULL trên một số platform. Fix dùng API thấp hơn:

```cpp
int idx = X509_get_ext_by_NID(cert, NID_basic_constraints, -1);
if (idx >= 0) {
    auto* ext = X509_get_ext(cert, idx);
    auto* bc  = static_cast<BASIC_CONSTRAINTS*>(X509V3_EXT_d2i(ext));
    if (bc) {
        info.is_ca = (bc->ca != 0);
        BASIC_CONSTRAINTS_free(bc);
    }
}
```

### 4.3 SHA-256 Length-Extension Attack

Nguyên lý: SHA-256 (Merkle-Damgård) dừng lại ở state H sau khi hash `key‖msg‖pad`. State này là 8 giá trị uint32 — chính là output hash. Ta có thể **tiếp tục nén** từ state đó để tính `SHA256(key‖msg‖pad‖ext)` mà không cần biết key:

```cpp
ExtResult extend(const vector<uint8_t>& hash_h,  // = SHA256(key‖msg) đã biết
                 size_t key_len,
                 const vector<uint8_t>& msg,
                 const vector<uint8_t>& extension)
{
    // 1. Khôi phục state từ hash_h (parse 8×uint32 big-endian)
    // 2. Tính padding của (key‖msg) với tổng length = key_len + msg.size()
    // 3. forged_message = msg ‖ padding ‖ extension
    // 4. Tiếp tục compress state với từng 64-byte block của extension
    // 5. Trả về (forged_hash, forged_message)
}
```

### 4.4 File streaming không OOM

```cpp
std::vector<uint8_t> hash_file(Algo a, const std::string& path, size_t outlen) {
    std::ifstream f(path, std::ios::binary);
    uint8_t chunk[65536];
    while (f.read(reinterpret_cast<char*>(chunk), sizeof(chunk)) || f.gcount() > 0)
        EVP_DigestUpdate(ctx, chunk, f.gcount());
    // ... Final / FinalXOF
}
```

---

## 5. KAT Validation

### 5.1 Nguồn vector

| File | Nguồn | Số vector |
|---|---|---|
| `kat/sha2.json` | NIST FIPS 180-4 Appendix B | 8 (SHA-224/256/384/512 × {"", "abc"}) |
| `kat/sha3.json` | NIST FIPS 202 Appendix A | 8 (SHA3-224/256/384/512 × {"", "abc"}) |
| `kat/shake.json` | NIST FIPS 202 | 6 (SHAKE128/256, nhiều outlen) |

### 5.2 Kết quả KAT

```
$ hashtool kat --file kat/sha2.json
PASS  sha224  ""          → d14a028c2a3a...
PASS  sha256  ""          → e3b0c44298fc...
PASS  sha256  "abc"       → ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
PASS  sha512  "abc"       → ddaf35a193617...
[8/8 PASS]

$ hashtool kat --file kat/sha3.json
[8/8 PASS]

$ hashtool kat --file kat/shake.json
[6/6 PASS]
```

**Xác nhận độc lập:** SHA-256("abc") verify qua OpenSSL CLI, Python `hashlib`, và .NET `SHA256` — tất cả cho cùng kết quả.

---

## 6. Negative Testing

### 6.1 Bảng test cases

| Test case | Input | Kỳ vọng | Kết quả |
|---|---|---|---|
| MD5 bị reject | `parse_algo("md5")` | throw `runtime_error` | PASS |
| SHA-1 bị reject | `parse_algo("sha1")` | throw `runtime_error` | PASS |
| SHAKE không outlen | `hash_bytes(SHAKE128, data, 0)` | throw `runtime_error` | PASS |
| File không tồn tại | `hash_file(SHA256, "/no/such")` | throw `runtime_error` | PASS |
| Garbage PEM | `load_cert("garbage bytes")` | throw `runtime_error` | PASS |
| Length-ext hash sai size | `extend(31_bytes, ...)` | throw `invalid_argument` | PASS |
| Length-ext sai key | `verify_attack(wrong_key, ...)` | return false | PASS |
| Cert tampered signature | `cert_verify(tampered)` | return false | PASS |

### 6.2 Kết quả test toàn bộ

```
$ ./lab4_tests --reporter compact
All tests passed (120 assertions in 64 test cases)
```

---

## 7. Performance Evaluation

### 7.1 Phương pháp benchmark

- **Platform:** Windows 11, Intel i7-1165G7 (Tiger Lake, SHA-NI extension), MSVC Release `/O2`
- **Warmup:** 3 lần trước khi đo
- **Số lần đo:** N = 5, lấy mean và 95% CI
- **Metric:** throughput (MB/s) và latency (ms)

### 7.2 Kết quả đo thực tế

| Thuật toán | Cấu trúc | Size | Mean (ms) | Throughput (MB/s) | 95% CI (ms) |
|---|---|---|---|---|---|
| SHA-256 | Merkle-Damgård | 1 MiB | 0.68 | **1464.60** | ±0.05 |
| SHA-256 | Merkle-Damgård | 100 MiB | 179.07 | 558.45 | ±25.62 |
| SHA-512 | Merkle-Damgård | 1 MiB | 5.13 | 194.86 | ±0.26 |
| SHA-512 | Merkle-Damgård | 100 MiB | 463.44 | 215.78 | ±29.56 |
| SHA3-256 | Keccak sponge | 1 MiB | 8.17 | 122.36 | ±0.79 |
| SHA3-256 | Keccak sponge | 100 MiB | 795.85 | 125.65 | ±70.60 |
| SHA3-512 | Keccak sponge | 1 MiB | 16.91 | 59.15 | ±0.78 |
| SHA3-512 | Keccak sponge | 100 MiB | 1592.25 | 62.80 | ±42.17 |

### 7.3 Phân tích

**SHA-256 nhanh nhất (~1465 MB/s tại 1 MiB):**
Intel Tiger Lake có extension SHA-NI (`SHA256RNDS2`, `SHA256MSG1`, `SHA256MSG2`) giúp OpenSSL thực hiện SHA-256 gần như ở tốc độ memory bandwidth.

**SHA-512 chậm hơn SHA-256 (~7.5×):**
SHA-512 dùng 64-bit words và 80 rounds thay vì 64. Không có SHA-512 hardware acceleration trên Tiger Lake (SHA-NI chỉ cover SHA-256/SHA-1). Trên CPU 64-bit không có HW, SHA-512 vẫn nhanh hơn SHA-256 trong lý thuyết (xử lý 2× data per round), nhưng thua kém nhiều khi SHA-256 có HW.

**SHA-3 chậm nhất (~60–122 MB/s):**
Keccak-f[1600] permutation không có hardware acceleration trên x86 phổ thông. Throughput ổn định qua 1 MiB và 100 MiB — không có caching artifact do chi phí mỗi block lớn hơn.

**So sánh trực quan:**

```
SHA-256   ████████████████████████████████████████████████ 1465 MB/s
SHA-512   ██████ 195 MB/s
SHA3-256  ████ 122 MB/s
SHA3-512  ██ 59 MB/s
```

---

## 8. Security Analysis

### 8.1 Merkle-Damgård và Length-Extension Attack

SHA-2 (SHA-256, SHA-512) dùng cấu trúc **Merkle-Damgård**: hash output = internal compression state sau block cuối. Điều này cho phép **length-extension attack**:

```
Biết:      H = SHA256(secret ‖ msg)
Tính được: SHA256(secret ‖ msg ‖ pad ‖ ext)  mà không biết secret
```

**Kịch bản tấn công thực tế:**
Nếu server dùng `MAC = SHA256(key ‖ request)` để xác thực API request, attacker có thể:
1. Quan sát `(request, MAC)` hợp lệ.
2. Tính `(request ‖ padding ‖ "&admin=true", forged_MAC)`.
3. Server verify forged_MAC thành công → escalate privilege.

**Mitigation:**
- Dùng **HMAC-SHA256** (không bị tấn công này vì có padding ngoài: `H(k_outer ‖ H(k_inner ‖ m))`).
- Hoặc dùng **SHA-3** (Keccak sponge không có state leak).

### 8.2 SHA-3 (Keccak Sponge) — kháng length-extension

SHA-3 dùng cấu trúc sponge: output được **squeeze** ra từ state, không phải state thô. Sau khi squeeze, sponge add domain separator (0x06) và XOR vào state — attacker không thể tiếp tục squeeze mà không biết toàn bộ state (1600 bit, phần capacity không lộ ra ngoài).

### 8.3 SHAKE XOF — truncation attack

SHAKE cho phép output length tùy ý. Nếu caller chỉ dùng 16 byte đầu từ SHAKE256 (security 128-bit thay vì 256-bit), collision resistance giảm xuống 2^64. `hashtool` yêu cầu `--outlen` explicit để buộc người dùng suy nghĩ về output size.

### 8.4 MD5 / SHA-1 bị loại bỏ hoàn toàn

| Thuật toán | Tình trạng | Lý do reject |
|---|---|---|
| MD5 | **Broken** | Collision tìm được trong vài giây (Wang et al. 2004) |
| SHA-1 | **Deprecated** | SHAttered collision (Stevens et al. 2017) |
| SHA-256+ | Secure | Chưa có collision attack thực tiễn |

`parse_algo()` throw ngay khi nhận "md5" hoặc "sha1" — fail-fast, không cho phép downgrade.

### 8.5 X.509 Certificate Trust

`X509_verify(cert, issuer_pubkey)` dùng EVP để xác minh chữ ký — bao phủ RSA, EC, và các algorithm hiện đại. Không tự implement crypto verify.

**Hạn chế (để report trung thực):**
- Chưa implement TLS deploy (Nginx + ECDSA cert + HTTPS screenshot).
- Chưa demo MD5 collision bằng hashclash artifacts.
- Chưa validate full certificate chain (chỉ verify cert-issuer 1 cấp).

### 8.6 Threat Model tóm tắt

| Threat | Mitigation trong lab |
|---|---|
| Weak hash (MD5/SHA-1) | `parse_algo` throw ngay |
| Length-extension (SHA-2 MAC) | Demo và giải thích; khuyến cáo dùng HMAC |
| SHAKE truncation | Yêu cầu explicit `outlen` |
| Cert tampering | `X509_verify` fail-closed |
| Large file OOM | Streaming 64 KB chunks |

---

## 9. Lessons Learned

### 9.1 Bug đã gặp và sửa

**Bug 1 — SHA-256("abc") hardcode sai:**
- **Biểu hiện:** `test_hash_kat` FAIL từ đầu.
- **Nguyên nhân:** Hardcode expected value sai trong KAT JSON.
- **Giá trị đúng:** `ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`.
- **Fix:** Sửa trong `kat/sha2.json` và verify qua OpenSSL CLI + Python `hashlib`.

**Bug 2 — X.509 IsCA không detect được:**
- **Biểu hiện:** `parse_cert_pem: CA flag` test FAIL — `is_ca` luôn `false` dù cert có `CA:TRUE`.
- **Nguyên nhân:** `X509V3_EXT_conf_nid(nullptr, &ctx, NID_basic_constraints, "critical,CA:TRUE")` trả NULL trên platform này → `X509_check_ca()` trả 0.
- **Fix:** Dùng `BASIC_CONSTRAINTS_new()` + `X509V3_EXT_i2d()` trong test; `X509_get_ext_by_NID` + `X509V3_EXT_d2i` trong cert_parser.

**Bug 3 — SHAKE outlen=0 gây UB:**
- **Nguyên nhân:** `EVP_DigestFinalXOF` với `outlen=0` gây undefined behavior.
- **Fix:** Check `outlen == 0` và throw `runtime_error` trước khi gọi OpenSSL.

### 9.2 Điều rút ra

1. **SHA-2 dễ bị length-extension vì thiết kế cũ:** Merkle-Damgård ra đời 1989 trước khi length-extension được hiểu đầy đủ. SHA-3 (2015) đã fix với sponge construction.
2. **Hardware acceleration quan trọng hơn tưởng:** SHA-256 nhanh gấp 24× SHA3-256 trên Tiger Lake — chênh lệch hoàn toàn do SHA-NI.
3. **Platform API unreliable:** `X509_check_ca` không nhất quán giữa các phiên bản — luôn parse extension trực tiếp khi quan trọng.
4. **Streaming là bắt buộc cho hash tool thực tế:** Hash file 1 GB cần stream 64 KB/lần.

---

## 10. Conclusion

Lab 4 đã hoàn thành đầy đủ yêu cầu chính và bonus:

| Hạng mục | Trạng thái |
|---|---|
| 10 hash algorithms (SHA-2, SHA-3, SHAKE) | ✓ |
| XOF output length tùy chỉnh | ✓ |
| Streaming file (không OOM) | ✓ |
| KAT NIST FIPS 180-4 / 202 (22 vectors) | ✓ All PASS |
| X.509 parsing (8+ fields) | ✓ |
| X.509 signature verify | ✓ |
| SHA-256 length-extension attack (Bonus +5) | ✓ |
| 64/64 tests PASS (120 assertions) | ✓ |
| MD5/SHA-1 rejection fail-fast | ✓ |
| Benchmark với 95% CI | ✓ |
| TLS deploy demo | Chưa làm (deferred) |
| MD5 collision demo (hashclash) | Chưa làm (deferred) |

---

## 11. References

1. **NIST FIPS 180-4** — "Secure Hash Standard (SHS)", NIST, 2015.
2. **NIST FIPS 202** — "SHA-3 Standard: Permutation-Based Hash and Extendable-Output Functions", NIST, 2015.
3. **Bertoni, G. et al.** — "Keccak reference", 2011.
4. **Wang, X. et al.** — "Finding Collisions in the Full SHA-1", CRYPTO 2005.
5. **Stevens, M. et al.** — "The First Collision for Full SHA-1 (SHAttered)", 2017.
6. **Kelsey, J. & Schneier, B.** — "Second Preimages on n-Bit Hash Functions for Much Less than 2^n Work", EUROCRYPT 2005.
7. **OpenSSL 3.x Documentation** — EVP_DigestInit, EVP_DigestFinalXOF, X509_verify, X509V3_EXT_d2i.
8. **RFC 2104** — "HMAC: Keyed-Hashing for Message Authentication", 1997.
9. **NIST ACVP** — Automated Cryptographic Validation Protocol test vectors.
