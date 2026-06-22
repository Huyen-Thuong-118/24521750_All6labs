# Chương 5 — Digital Signatures: ECDSA-P256 & RSA-PSS-3072

> **Sinh viên:** Nguyễn Đỗ Ngọc Huyền Thương · **MSV:** 24521750
> **Tool:** `sigtool` · **Thư viện:** OpenSSL 3.6.2
> **Ngày hoàn thành:** 2026-06-22

---

## 1. Objectives

### 1.1 Mục tiêu lab

Lab 5 yêu cầu xây dựng CLI tool `sigtool` thực hiện ký số và xác thực chữ ký với hai thuật toán:

- **ECDSA-P256** với nonce **RFC 6979 deterministic** (FIPS 186-4, RFC 6979)
- **RSA-PSS-3072** SHA-256, salt = hashLen = 32 bytes (PKCS#1 v2.2 / RFC 8017)

Bao gồm: keygen PEM, sign/verify detached signature, encoding raw/base64, batch verify, và so sánh hiệu năng hai thuật toán.

### 1.2 Những gì đã xây dựng

| Thành phần | Mô tả |
|---|---|
| `sigtool keygen ecdsa\|rsa` | Sinh keypair PEM (private + public) |
| `sigtool sign ecdsa\|rsa` | Ký file, xuất DER hoặc base64 |
| `sigtool verify ecdsa\|rsa` | Xác thực chữ ký, trả về VALID/INVALID |
| RFC 6979 tự implement | HMAC-SHA256 derivation nonce k trong `ecdsa_signer.cpp` |
| Batch verify | `verify_batch()` xử lý N cặp (msg, sig) |
| Encoding | Base64 encode/decode qua OpenSSL BIO chain |
| 39/39 Catch2 tests | test_ecdsa, test_rsapss, test_negative, test_batch |
| Benchmark | Keygen/sign/verify tại 1KiB/16KiB/1MiB cho cả hai algo |

### 1.3 Lý do thiết kế

RFC 6979 loại bỏ hoàn toàn sự phụ thuộc vào RNG trong bước ký — đây là bài học trực tiếp từ vụ PS3 (Sony sử dụng nonce cố định k, để lộ private key). Tự implement giúp hiểu rõ cơ chế HMAC-DRBG trong ký số.

---

## 2. Environment

### 2.1 Môi trường thử nghiệm

| Thành phần | Chi tiết |
|---|---|
| **OS** | Windows 11 Home 10.0.26200 |
| **CPU** | Intel Core i7-1165G7 @ 2.80 GHz (Tiger Lake, 4C/8T, AES-NI) |
| **RAM** | 8 GB |
| **Compiler** | MSVC 2022 (v17.14) |
| **CMake** | 3.25+ |
| **OpenSSL** | 3.6.2 (via vcpkg) |
| **Catch2** | 3.5.x (via vcpkg) |
| **Build flags** | `/O2 /std:c++17` (Release) |

### 2.2 Phụ thuộc & Build

```bash
# Windows
vcpkg install openssl catch2
cmake -B build -DCMAKE_TOOLCHAIN_FILE=... -DCMAKE_BUILD_TYPE=Release
cmake --build build --target sigtool lab5_tests lab5_bench --config Release
```

---

## 3. System Design

### 3.1 Kiến trúc

```
sigtool (CLI)
    │
    ├── sig5::keygen(Algo)          ─── ecdsa_signer.cpp / rsapss_signer.cpp
    ├── sig5::sign_msg(Algo, ...)   ─── ecdsa_sign() / rsapss_sign()
    ├── sig5::verify_msg(Algo, ...) ─── ecdsa_verify() / rsapss_verify()
    ├── sig5::verify_batch(...)     ─── sig_dispatch.cpp
    └── sig5::sig_to/from_base64() ─── OpenSSL BIO chain

ecdsa_signer.cpp:
    SHA256(msg) → hash[32]
    rfc6979_k(priv_bn, hash, order) → k (BIGNUM)
    do_sign(key, hash):
        R = k·G (EC_POINT_mul)
        r = R.x mod q
        s = k⁻¹·(h + r·x) mod q
        → ECDSA_SIG → DER encode

rsapss_signer.cpp:
    EVP_DigestSignInit(RSA_PKCS1_PSS_PADDING, MGF1-SHA256, salt=32)
    EVP_DigestSignUpdate + EVP_DigestSignFinal
```

### 3.2 RFC 6979 — HMAC-SHA256 nonce derivation

```
K = 0x00…00 (32 bytes)
V = 0x01…01 (32 bytes)
K = HMAC_K(V ‖ 0x00 ‖ int2octets(x) ‖ bits2octets(h₁))
V = HMAC_K(V)
K = HMAC_K(V ‖ 0x01 ‖ int2octets(x) ‖ bits2octets(h₁))
V = HMAC_K(V)
loop:
    V = HMAC_K(V)
    k = bits2int(V)
    if 1 ≤ k < q: return k
    K = HMAC_K(V ‖ 0x00); V = HMAC_K(V)
```

- `int2octets(x)`: private key → 32-byte big-endian (`BN_bn2binpad`)
- `bits2octets(h₁)`: SHA-256(msg) mod q → 32-byte big-endian
- Với P-256 + SHA-256: q và hash cùng 256 bit → một vòng HMAC là đủ (k ≈ luôn hợp lệ)

### 3.3 Tham số

| Thuật toán | Curve/Key | Hash | Salt | Sig format | Sig size |
|---|---|---|---|---|---|
| ECDSA-P256 | NIST P-256 | SHA-256 | — | DER SEQUENCE(r,s) | ~70–72 B |
| RSA-PSS-3072 | RSA-3072 | SHA-256 | 32 B | Raw modulus | 384 B |

---

## 4. Implementation

### 4.1 ECDSA với RFC 6979

`ecdsa_signer.cpp` sử dụng OpenSSL low-level EC API (deprecated in 3.x, suppressed `/wd4996`) vì đây là cách duy nhất để inject custom nonce vào ECDSA signing — EVP API không cho phép override RNG ở cấp nonce.

```cpp
// RFC 6979: HMAC-SHA256 one-shot over concatenated parts
static void hmac_sha256(const uint8_t* K, size_t Klen,
    std::initializer_list<std::pair<const uint8_t*, size_t>> parts,
    uint8_t out[32])
{
    unsigned char buf[4096]; size_t total = 0;
    for (auto& [d, n] : parts) { memcpy(buf + total, d, n); total += n; }
    unsigned int outlen = 32;
    HMAC(EVP_sha256(), K, (int)Klen, buf, total, out, &outlen);
}
```

Sau khi derive k, tính ECDSA thủ công:

```cpp
EC_POINT_mul(grp, R, k, nullptr, nullptr, ctx.get()); // R = k·G
EC_POINT_get_affine_coordinates(grp, R, r, nullptr, ctx.get());
BN_mod(r, r, order, ctx.get());                        // r = R.x mod q
// s = k⁻¹·(h + r·x) mod q
BN_mod_mul(s, r, x, order, ctx.get());
BN_mod_add(s, s, h_bn, order, ctx.get());
BN_mod_mul(s, s, kinv, order, ctx.get());
```

### 4.2 RSA-PSS với OpenSSL EVP

```cpp
void set_pss_params(EVP_PKEY_CTX* ctx) {
    EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PSS_PADDING);
    EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256());
    EVP_PKEY_CTX_set_rsa_pss_saltlen(ctx, 32);  // salt = hashLen = 32 B
}
```

PSS sử dụng random salt mỗi lần → mỗi lần ký cho ra chữ ký khác nhau, nhưng tất cả đều verify được với cùng public key.

### 4.3 Encoding

Base64 dùng OpenSSL BIO chain:

```cpp
BIO* b64 = BIO_new(BIO_f_base64());
BIO* mem = BIO_new(BIO_s_mem());
BIO_push(b64, mem);
BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // no line breaks
BIO_write(b64, data, len);
BIO_flush(b64);
```

`BIO_FLAGS_BASE64_NO_NL` đảm bảo output là single-line, dễ embed vào JSON/header.

---

## 5. KAT Validation

### 5.1 RFC 6979 Determinism Test

Không có NIST ACVP vector cho RFC 6979 ECDSA được tích hợp trực tiếp, nhưng tính correctness được kiểm chứng bằng **determinism test**:

```
cùng (key, msg) → ký 2 lần → sig1 == sig2 (byte-for-byte)
```

Đây là điều kiện cần và đủ của RFC 6979: nếu nonce k được derive đúng từ HMAC-SHA256 của (priv, hash), kết quả luôn xác định.

| Test | Input | Expected | Kết quả |
|---|---|---|---|
| Determinism: same key+msg | "deterministic ECDSA test" | sig1 == sig2 | **PASS** |
| Different messages | "msg A" vs "msg B" | sig1 ≠ sig2 | **PASS** |
| Different keys | key1 vs key2 | sig1 ≠ sig2 | **PASS** |

### 5.2 Round-trip Correctness

| Test | Mô tả | Kết quả |
|---|---|---|
| ECDSA short msg | "hello ECDSA-P256" | **PASS** |
| ECDSA empty msg | `{}` | **PASS** |
| ECDSA 64 KiB | 65536 bytes 0xAB | **PASS** |
| RSA-PSS short msg | "hello RSA-PSS-3072" | **PASS** |
| RSA-PSS empty msg | `{}` | **PASS** |
| RSA-PSS 64 KiB | 65536 bytes 0xCD | **PASS** |

### 5.3 Format Correctness

| Test | Expected | Kết quả |
|---|---|---|
| ECDSA sig starts with 0x30 | DER SEQUENCE tag | **PASS** |
| RSA-PSS sig size | 384 bytes (= 3072/8) | **PASS** |
| Base64 encode → decode → verify | roundtrip valid | **PASS** |

---

## 6. Negative Testing

| Test case | Input | Expected | Kết quả |
|---|---|---|---|
| Tamper message (ECDSA) | msg[0] ^= 0xFF | verify = false | **PASS** |
| Tamper signature (ECDSA) | sig[0] ^= 0xFF | verify = false | **PASS** |
| Wrong key (ECDSA) | sign key1, verify key2 | verify = false | **PASS** |
| Garbage DER (ECDSA) | `{0x30,0x06,...}` | verify = false (no throw) | **PASS** |
| Tamper message (RSA-PSS) | msg[0] ^= 0xFF | verify = false | **PASS** |
| Tamper signature (RSA-PSS) | sig[0] ^= 0xFF | verify = false | **PASS** |
| Wrong key (RSA-PSS) | sign key1, verify key2 | verify = false | **PASS** |
| Garbage 384B signature | `std::vector(384, 0xFF)` | verify = false (no throw) | **PASS** |
| Cross-algo: ECDSA sig → RSA verifier | ~72B sig vs 384B expected | false | **PASS** |
| Cross-algo: RSA sig → ECDSA verifier | 384B sig, EC pubkey | false | **PASS** |
| Empty signature (ECDSA) | `{}` | false | **PASS** |
| Empty signature (RSA-PSS) | `{}` | false | **PASS** |
| Truncated DER (ECDSA) | sig[:half] | false | **PASS** |
| Garbage PEM pubkey | invalid PEM | false (no throw) | **PASS** |
| base64 decode garbage | "!!!not-base64!!!" | throw | **PASS** |

**Tất cả 39/39 tests PASS — verify_msg() hoàn toàn fail-closed.**

---

## 7. Performance Evaluation

### 7.1 Phương pháp

- Warmup: 3 lần trước đo
- N = 50 reps cho ECDSA (1 KiB, 16 KiB), N = 10 (1 MiB)
- N = 5 reps cho RSA-PSS (keygen chậm ~170ms)
- Môi trường: Windows 11 Release build, không pin governor (consumer laptop)

### 7.2 Kết quả ECDSA-P256 (RFC 6979)

| Message size | Keygen (ms) | Sign (ms) | Verify (ms) | Reps |
|---|---|---|---|---|
| 1 KiB | 0.32 | **0.107** | 0.190 | 50 |
| 16 KiB | 0.05 | 0.124 | 0.142 | 50 |
| 1 MiB | 0.05 | 0.707 | 1.020 | 10 |

> Keygen chỉ một lần (curve setup), sau đó sign rất nhanh ~0.1ms. Thời gian tăng theo msg chủ yếu do SHA-256 hash message.

### 7.3 Kết quả RSA-PSS-3072 SHA-256

| Message size | Keygen (ms) | Sign (ms) | Verify (ms) | Reps |
|---|---|---|---|---|
| 1 KiB | 170 | 1.890 | **0.100** | 5 |
| 16 KiB | 177 | 1.831 | 0.132 | 5 |
| 1 MiB | 166 | 7.909 | 2.183 | 3 |

> Verify RSA nhanh (pubkey op, chỉ modular exponentiation với e=65537). Sign chậm hơn (privkey op). Keygen cực chậm (~170ms) do sinh số nguyên tố 3072-bit.

### 7.4 So sánh

| Tiêu chí | ECDSA-P256 | RSA-PSS-3072 |
|---|---|---|
| Keygen | **0.3 ms** | 170 ms (570× chậm hơn) |
| Sign (1 KiB) | **0.11 ms** | 1.89 ms (17× chậm hơn) |
| Verify (1 KiB) | 0.19 ms | **0.10 ms** (1.9× nhanh hơn) |
| Pubkey size | **64 B** | 398 B (6× lớn hơn) |
| Sig size | **~72 B** | 384 B (5× lớn hơn) |
| Quantum safe | ✗ | ✗ |
| Deterministic | **✓ (RFC 6979)** | ✗ (random PSS salt) |

**Nhận xét:**
- ECDSA phù hợp khi cần sign nhanh, key nhỏ, và determinism (embedded system, IoT)
- RSA-PSS phù hợp khi verify nhiều lần (server verify client sig) — verify rất nhanh vì exponent công khai nhỏ (e=65537)
- Cả hai đều không kháng lượng tử → xem Lab 6 (ML-DSA-44) để so sánh

---

## 8. Security Analysis

### 8.1 ECDSA và nguy cơ nonce k

Lỗ hổng **nonce reuse** trong ECDSA là thảm hoạ:
- Nếu dùng cùng k cho hai message khác nhau: `s₁ = k⁻¹(h₁ + r·x)` và `s₂ = k⁻¹(h₂ + r·x)`
- Từ `s₁ - s₂ = k⁻¹(h₁ - h₂)` → tính được `k = (h₁ - h₂)/(s₁ - s₂)` → tính được private key x
- **Thực tế:** Vụ PS3 (2010) — Sony dùng k cố định, hacker khôi phục private key signing firmware

**RFC 6979 fix:** k = HMAC-SHA256(private_key, message_hash) — hoàn toàn deterministic, không phụ thuộc RNG. Ngay cả khi RNG bị tấn công, chữ ký vẫn an toàn.

### 8.2 RSA-PSS vs PKCS#1 v1.5

| | RSA-PKCS1-v1.5 | RSA-PSS |
|---|---|---|
| Security proof | Không có | Có (random oracle model) |
| Padding oracle | Dễ tấn công | Không có oracle |
| Deterministic | Có | Không (random salt) |
| FIPS approved | Legacy | ✓ (FIPS 186-5) |

PSS salt = hashLen = 32B là tham số tối ưu: đủ entropy để bảo vệ, không waste space.

### 8.3 DER encoding của ECDSA

ECDSA signature DER = `SEQUENCE { INTEGER r, INTEGER s }`. Mỗi INTEGER được DER-encode với length prefix, và nếu MSB = 1 thì thêm byte 0x00 để tránh nhầm với số âm. Đây là lý do sig size dao động 70–72 bytes (thay vì luôn 64 bytes).

**Malleability:** ECDSA DER không unique — kẻ tấn công có thể tạo signature hợp lệ thứ hai bằng cách negate s → `s' = q - s`. `verify()` cũng chấp nhận (r, s'). Để fix: enforce `s ≤ q/2` (low-S normalization, như Bitcoin). Lab này không implement low-S nhưng đây là điểm cần biết cho production.

### 8.4 Threat Model

| Mối đe dọa | Biện pháp |
|---|---|
| Nonce k reuse / RNG weak | RFC 6979 deterministic nonce |
| Message tampering | Verify sẽ fail (ECDSA/PSS đều integrity-protected) |
| Key extraction từ side-channel | Không implement constant-time (lab giới hạn) |
| Quantum adversary | Cả hai đều VULNERABLE → cần Lab 6 ML-DSA-44 |
| Cross-algo substitution | verify_msg kiểm tra algo mismatch → false |

### 8.5 Hạn chế

- OpenSSL deprecated EC API (`EC_KEY_*`, `ECDSA_*`) — sẽ bị remove trong OpenSSL 4.x
- Không implement constant-time ECDSA (timing side-channel có thể leak private key trên shared hardware)
- Không có low-S normalization cho ECDSA

---

## 9. Lessons Learned

### 9.1 Bug gặp phải

**Bug 1: `BN_value_one()` trả `const BIGNUM*` nhưng `EVP_PKEY_CTX_set_rsa_keygen_pubexp` nhận `BIGNUM*`**
- Biểu hiện: compile error C2664 trong `rsapss_signer.cpp`
- Fix: Bỏ lời gọi `set_rsa_keygen_pubexp` không cần thiết — default exponent đã là 65537

**Bug 2: RFC 6979 `bits2octets(h₁)` — cần reduce mod q trước**
- Biểu hiện: Nếu hash ≥ q, k sẽ sai → signature không verify
- Fix: `if (BN_cmp(h1, q) >= 0) BN_sub(h1, h1, q)` trước khi pad → `bh[32]`
- Với P-256 + SHA-256: hash 256-bit, q ~256-bit → xác suất h1 ≥ q nhỏ nhưng phải handle

**Bug 3: `sig_from_base64` trả error khi input rỗng hoặc có newline**
- Biểu hiện: Test decode garbage throws — "base64 decode failed" với n ≤ 0
- Fix: `BIO_FLAGS_BASE64_NO_NL` và kiểm tra `n > 0` trước khi resize

### 9.2 Điều rút ra

1. **RFC 6979 không khó implement nhưng dễ sai ở chi tiết:** `bits2octets` khác `int2octets`; `bits2int` vs `bits2octets` là hai phép khác nhau trong RFC.
2. **OpenSSL EVP API không cho phép inject custom nonce:** Phải dùng deprecated low-level API để implement RFC 6979 — đây là trade-off thực tế giữa tính đúng đắn (RFC 6979) và tính forward-compatible (EVP).
3. **PSS randomness là tính năng, không phải bug:** Nhiều developer nhầm khi thấy mỗi lần sign ra kết quả khác — PSS salt tạo tính mới (freshness) và bảo vệ khỏi multi-target attacks.
4. **Batch verify không tự động nhanh hơn single verify:** Với ECDSA, mỗi signature cần EC point arithmetic độc lập — batch verify ở đây là sequential, không phải batch crypto.

---

## 10. Conclusion

Lab 5 đã xây dựng thành công `sigtool` với:
- **RFC 6979** tự implement (HMAC-SHA256 chain) → nonce k hoàn toàn deterministic, đã verify byte-for-byte
- **ECDSA-P256** keygen/sign/verify với DER format và base64 encoding
- **RSA-PSS-3072** SHA-256 salt=32B theo PKCS#1 v2.2
- **39/39 Catch2 tests PASS**: round-trip, determinism, negative, cross-algo, batch
- **Benchmark**: ECDSA sign ~0.11ms (nhanh), RSA-PSS keygen ~170ms (chậm); RSA-PSS verify ~0.10ms (rất nhanh)

**Hướng phát triển:**
- Migrate ECDSA sang OpenSSL 3.x EVP API khi OpenSSL cung cấp deterministic signing interface
- Implement low-S normalization cho ECDSA (Bitcoin-style) để ngăn signature malleability
- Constant-time EC scalar multiplication (chống timing attack trên shared hardware)
- Thêm Ed25519 (EdDSA) — deterministic by design, nhanh hơn ECDSA, và không cần implement RFC 6979 riêng

---

## 11. References

1. **RFC 6979** — "Deterministic Usage of the Digital Signature Algorithm (DSA) and Elliptic Curve Digital Signature Algorithm (ECDSA)" (T. Pornin, 2013).
2. **FIPS 186-4** — "Digital Signature Standard (DSS)" (NIST, 2013). Section 6: ECDSA.
3. **FIPS 186-5** — "Digital Signature Standard (DSS)" (NIST, 2023). Drops RSA-PKCS1, recommends RSA-PSS.
4. **RFC 8017** — "PKCS #1: RSA Cryptography Specifications Version 2.2" (K. Moriarty et al., 2016). Section 9.1: RSASSA-PSS.
5. **SEC 2: Recommended Elliptic Curve Domain Parameters** — Certicom Research, 2010. P-256 (secp256r1) parameters.
6. **"The PlayStation 3 Jailbreak"** — Geohot et al. (2010). Minh chứng nonce reuse dẫn đến key recovery.
7. **OpenSSL 3.6.2 Documentation** — EVP_DigestSign API, RSA PSS padding, ECDSA low-level API (deprecated).
8. **"ECDSA is not that hard"** — Andrea Corbellini blog series. EC arithmetic và ECDSA từ đầu.
9. **NIST SP 800-186** — "Recommendations for Discrete Logarithm-based Cryptography: Elliptic Curve Domain Parameters" (2023). Xác nhận P-256 là approved curve.
10. **"Seriously, stop using RSA"** — Thomas Ptacek (2015). Lập luận cho EC over RSA về key/sig size và security margins.
