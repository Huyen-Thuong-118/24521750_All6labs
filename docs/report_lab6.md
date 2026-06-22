# Chương 6 — Post-Quantum Cryptography: ML-DSA-44 & ML-KEM-512

> **Sinh viên:** Nguyễn Đỗ Ngọc Huyền Thương · **MSV:** 24521750
> **Tool:** `pqtool` · **Thư viện:** OpenSSL 3.6.2 (native ML-DSA/ML-KEM provider)
> **Ngày hoàn thành:** 2026-06-22

---

## 1. Objectives

### 1.1 Mục tiêu lab

Lab 6 yêu cầu xây dựng CLI tool `pqtool` triển khai mật mã **hậu lượng tử (Post-Quantum Cryptography — PQC)**:

- **ML-DSA-44** (Module-Lattice-Based Digital Signature Algorithm, FIPS 204): keygen, sign, verify
- **ML-KEM-512** (Module-Lattice-Based Key-Encapsulation Mechanism, FIPS 203): keygen, encaps, decaps
- **PQ Certificate mini-project**: CA dùng ML-DSA-44 ký public key của subject, lưu dạng JSON
- So sánh hiệu năng và kích thước với classical (ECDSA-P256, RSA-PSS-3072 từ Lab 5)

### 1.2 Những gì đã xây dựng

| Thành phần | Mô tả |
|---|---|
| `pqtool keygen mldsa44` | Sinh keypair ML-DSA-44, xuất PEM |
| `pqtool keygen mlkem512` | Sinh keypair ML-KEM-512, xuất PEM |
| `pqtool sign/verify mldsa44` | Ký/xác minh detached signature (raw hoặc base64) |
| `pqtool encaps/decaps mlkem512` | Encapsulation/Decapsulation ML-KEM-512, xuất ciphertext + shared secret |
| `pqtool cert issue/verify` | PQ Certificate JSON: CA ký sub public key bằng ML-DSA-44 |
| `lab6_tests` | 44 test cases, 70 assertions — 100% PASS |
| `lab6_bench` | Benchmark ML-DSA/ML-KEM + bảng so sánh Lab 5 |

### 1.3 Lý do thiết kế

OpenSSL 3.6.2 đã tích hợp ML-DSA và ML-KEM natively theo FIPS 203/204 — **không cần liboqs**. Đây là backend lý tưởng: API quen thuộc (EVP), bảo mật được kiểm chứng, không thêm dependency bên ngoài.

PQ Certificate dùng JSON thay vì X.509 đầy đủ vì mục tiêu là minh họa cơ chế PKI hậu lượng tử (CA ký public key bằng lattice), không phải triển khai TLS production.

---

## 2. Environment

### 2.1 Môi trường thử nghiệm

| Thành phần | Chi tiết |
|---|---|
| **OS** | Windows 11 Home 10.0.26200 |
| **CPU** | Intel Core i7-1165G7 @ 2.80 GHz (Tiger Lake, 4 cores / 8 threads) |
| **RAM** | 8 GB |
| **Compiler** | MSVC 2022 (cl.exe) — Release `/O2` |
| **CMake** | 3.28+ |
| **OpenSSL** | **3.6.2** — native ML-DSA/ML-KEM provider |
| **nlohmann/json** | 3.11.3 (via vcpkg) — PQ cert serialization |
| **Catch2** | 3.5.x (via vcpkg) — unit tests |

### 2.2 Kiểm tra ML-DSA/ML-KEM availability

```powershell
openssl list -signature-algorithms | findstr ML-DSA
# { 2.16.840.1.101.3.4.3.17, id-ml-dsa-44, ML-DSA-44, MLDSA44 } @ default

openssl list -kem-algorithms | findstr ML-KEM
# { 2.16.840.1.101.3.4.4.1, id-alg-ml-kem-512, ML-KEM-512, MLKEM512 } @ default
```

### 2.3 Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pqtool lab6_tests lab6_bench --config Release
```

---

## 3. System Design

### 3.1 Kiến trúc tổng thể

```
                    ┌──────────── pqtool (CLI) ─────────────┐
 keygen mldsa44  ──►│ mldsa_keygen() → PEM priv/pub         │
 sign/verify     ──►│ mldsa_sign/verify() — pure ML-DSA-44  │──► sig (raw/b64)
 keygen mlkem512 ──►│ mlkem_keygen() → PEM priv/pub         │
 encaps/decaps   ──►│ mlkem_encaps/decaps() — FO transform  │──► ct + ss
 cert issue      ──►│ cert_issue(): CA signs sub pubkey      │
 cert verify     ──►│ cert_verify(): ML-DSA verify JSON      │──► VALID/INVALID
                    └───────────────────────────────────────┘
       Backend: OpenSSL 3.6.2 EVP_PKEY_CTX (native FIPS 203/204 provider)
```

### 3.2 Tham số FIPS 203/204

| Tham số | ML-DSA-44 | ML-KEM-512 |
|---------|-----------|-----------|
| Security level | NIST Level 2 (~128-bit quantum) | NIST Level 1 (~128-bit quantum) |
| Public key | **1312 B** | **800 B** |
| Private key | 2560 B | 1632 B |
| Signature / Ciphertext | **2420 B** | **768 B** |
| Shared secret | — | 32 B |
| Internal lattice | Module-LWE (k=4, ℓ=4) | Module-LWE (k=2) |

### 3.3 Cấu trúc file

```
lab6_post_quantum/
├── include/pqtool.hpp       — API declarations (mldsa_*, mlkem_*, cert_*, base64)
├── src/
│   ├── mldsa.cpp            — ML-DSA-44 via OpenSSL EVP
│   ├── mlkem.cpp            — ML-KEM-512 via OpenSSL EVP
│   ├── pq_cert.cpp          — PQ Certificate JSON (issue + verify)
│   ├── pq_dispatch.cpp      — base64 encode/decode (OpenSSL BIO)
│   └── main.cpp             — CLI dispatcher
├── tests/
│   ├── test_mldsa.cpp       — 15 test cases
│   ├── test_mlkem.cpp       — 10 test cases
│   ├── test_cert.cpp        — 9 test cases
│   └── test_negative.cpp    — 10 test cases
└── benchmark/bench_main.cpp — latency + comparison table
```

---

## 4. Implementation

### 4.1 ML-DSA-44 — keygen

```cpp
EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_name(nullptr, "ML-DSA-44", nullptr);
EVP_PKEY_keygen_init(ctx);
EVP_PKEY* pkey = nullptr;
EVP_PKEY_keygen(ctx, &pkey);
// Xuất PEM:
PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0, nullptr, nullptr);
PEM_write_bio_PUBKEY(bio, pkey);
```

### 4.2 ML-DSA-44 — sign (pure ML-DSA, không external pre-hash)

```cpp
EVP_MD_CTX* ctx = EVP_MD_CTX_new();
// nullptr cho MD → pure ML-DSA (FIPS 204 §5), hedged với fresh randomness
EVP_DigestSignInit(ctx, nullptr, nullptr, nullptr, priv_key);
size_t siglen = 0;
EVP_DigestSign(ctx, nullptr, &siglen, msg.data(), msg.size()); // get size
EVP_DigestSign(ctx, sig.data(), &siglen, msg.data(), msg.size()); // sign
```

Dùng `nullptr` cho message digest vì ML-DSA tự hash nội bộ (FIPS 204 §5.2 "hedged" variant: kết hợp randomness + message + private key để tạo commitment — khác với ECDSA phải quản lý nonce bên ngoài).

### 4.3 ML-DSA-44 — verify

```cpp
EVP_MD_CTX* ctx = EVP_MD_CTX_new();
EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pub_key);
int rc = EVP_DigestVerify(ctx, sig.data(), sig.size(), msg.data(), msg.size());
// rc == 1 → valid; rc == 0 → invalid sig (không throw)
return rc == 1;
```

### 4.4 ML-KEM-512 — encaps

```cpp
EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_from_pkey(nullptr, pub_pkey, nullptr);
EVP_PKEY_encapsulate_init(ctx, nullptr);
size_t ctlen = 0, sslen = 0;
EVP_PKEY_encapsulate(ctx, nullptr, &ctlen, nullptr, &sslen); // query sizes
// ctlen = 768, sslen = 32
EVP_PKEY_encapsulate(ctx, ct.data(), &ctlen, ss.data(), &sslen);
```

### 4.5 ML-KEM-512 — decaps (implicit rejection)

```cpp
size_t sslen = OSSL_ML_KEM_SHARED_SECRET_BYTES; // = 32
std::vector<uint8_t> ss(sslen);
// Không bao giờ throw dù ct sai — trả pseudo-random ss (FO transform)
EVP_PKEY_decapsulate(ctx, ss.data(), &sslen, ct.data(), ct.size());
ss.resize(sslen);
```

Quyết định thiết kế: **không** throw khi decaps thất bại để tuân thủ FIPS 203 Implicit Rejection — tránh timing oracle leak.

### 4.6 PQ Certificate — issue

```cpp
// Canonical bytes for signing
std::string s = cert.subject + "|" + cert.public_key + "|" + cert.issuer;
auto data = std::vector<uint8_t>(s.begin(), s.end());
auto sig  = mldsa_sign(ca_priv_pem, data);
cert.signature = to_base64(sig);
```

Kết quả JSON:
```json
{
  "subject":    "CN=test.lab6",
  "public_key": "<base64-DER ML-DSA-44 pubkey>",
  "issuer":     "CN=Lab6CA",
  "signature":  "<base64 ML-DSA-44 signature>"
}
```

---

## 5. KAT Validation

### 5.1 Nguồn vector

Lab 6 sử dụng correctness testing thay vì byte-for-byte ACVP vectors. Lý do: ML-DSA hedged variant dùng fresh randomness mỗi lần ký → signature output **không deterministic** → không thể so sánh byte-for-byte với vector tĩnh. OpenSSL 3.6.2 đã được NIST validate theo FIPS 203/204; KAT tập trung vào **tính nhất quán của API wrapper**.

| Test | Phương pháp | Kết quả |
|------|------------|---------|
| ML-DSA round-trip | sign → verify cùng msg/key | PASS (15 tests) |
| ML-DSA sig size | `sig.size() == 2420` | PASS |
| ML-DSA batch 50 verify | 50 cặp (msg, sig) hợp lệ | PASS (count = 50) |
| ML-KEM ss match | `ss_encaps == ss_decaps` (5 cặp độc lập) | PASS |
| ML-KEM ct size | `ct.size() == 768` | PASS |
| ML-KEM ss size | `ss.size() == 32` | PASS |
| PQ Cert roundtrip | issue → to_json → from_json → verify | PASS |

### 5.2 Chạy tests

```
lab6_tests.exe --reporter compact
RNG seed: 1526301256
All tests passed (70 assertions in 44 test cases)
```

---

## 6. Negative Testing

### 6.1 ML-DSA negative cases

| Ca test | Input | Kết quả mong đợi | Thực tế |
|---------|-------|-----------------|---------|
| Tampered message | `msg[0] ^= 0xFF` | verify = false | PASS |
| Tampered signature | `sig[0] ^= 0xFF` | verify = false | PASS |
| Wrong key | sign key1, verify key2 | verify = false | PASS |
| Garbage sig (2420 bytes 0x42) | — | verify = false, no throw | PASS |
| Garbage pub PEM | invalid base64 | throws | PASS |
| Empty signature | `sig = {}` | verify = false | PASS |
| Truncated sig (100 bytes) | `sig.resize(100)` | verify = false | PASS |
| Sig of different message | sign "A", verify "B" | false | PASS |
| Batch with 1 tampered | 10 sigs, #4 flipped | count = 9 | PASS |
| Empty batch | `{}` | count = 0 | PASS |

### 6.2 ML-KEM negative cases

| Ca test | Input | Kết quả mong đợi | Thực tế |
|---------|-------|-----------------|---------|
| Wrong private key | encaps pub1, decaps priv2 | ss_wrong ≠ ss_correct | PASS |
| Modified ciphertext | `ct[0] ^= 0xFF` | ss_bad ≠ ss_orig, no throw | PASS |
| Empty ciphertext | `ct = {}` | no throw (implicit rejection) | PASS |
| Short ciphertext (10 bytes) | `bad_ct` | no throw, no crash | PASS |

**Điểm quan trọng:** ML-KEM implicit rejection đảm bảo `mlkem_decaps` **không bao giờ throw** kể cả khi ciphertext hoàn toàn sai — đây là thuộc tính bảo mật cốt lõi (tránh oracle attack).

### 6.3 PQ Certificate negative cases

| Ca test | Thao tác | Kết quả |
|---------|---------|---------|
| Wrong CA key | verify với pub của CA khác | false |
| Tamper subject | `subject = "CN=fake"` | false |
| Tamper public_key | `public_key[0] ^= 1` | false |
| Tamper issuer | `issuer = "CN=EvilCA"` | false |
| Tamper signature | `sig[0] ^= 1` → invalid base64 → caught | false |
| ML-KEM key for signing | cert_issue với ML-KEM priv | throws |
| JSON roundtrip | to_json → from_json → verify | true |

---

## 7. Performance Evaluation

### 7.1 Phương pháp

- **Platform:** Windows 11, Intel i7-1165G7 @ 2.80 GHz, RAM 8 GB
- **Compiler:** MSVC 2022, Release `/O2`
- **Runs:** N=30 (ML-DSA 1KiB), N=20 (16KiB), N=5 (1MiB), N=50 (ML-KEM)
- **Metric:** mean latency per operation (ms)

### 7.2 ML-DSA-44 Benchmark

| Msg size | Keygen (ms) | Sign (ms) | Verify (ms) |
|----------|------------|----------|------------|
| 1 KiB | 0.41 | **1.459** | 0.223 |
| 16 KiB | 0.28 | 0.878 | 0.212 |
| 1 MiB | 0.18 | 3.176 | 2.466 |

Nhận xét:
- **Keygen** nhanh (~0.2–0.4 ms) do lattice key generation chỉ cần sampling từ small secret distribution.
- **Sign** phụ thuộc message size (phải hash toàn bộ message); phần lattice cố định; overhead tăng ở 1MiB chủ yếu do hashing.
- **Verify** nhanh hơn sign — thuận lợi cho use case server-side verification cao tải.

### 7.3 ML-KEM-512 Benchmark

| Operation | Latency (ms) | Throughput (ops/s) |
|-----------|-------------|-------------------|
| Keygen | 0.13 | ~7,700 |
| Encaps | **0.068** | ~14,700 |
| Decaps | 0.118 | ~8,500 |

ML-KEM-512 cực kỳ nhanh — encaps chỉ mất 68 µs, nhanh hơn hầu hết thao tác crypto cổ điển.

### 7.4 Bảng so sánh chéo (Lab 5 classical vs Lab 6 PQC)

#### Chữ ký số

| Scheme | Sign (ms) | Verify (ms) | Pubkey (B) | Sig size (B) | Quantum safe? |
|--------|----------|------------|-----------|------------|--------------|
| ECDSA-P256 (Lab 5) | **0.11** | 0.19 | 64 | **72** | ✗ |
| RSA-PSS-3072 (Lab 5) | 1.90 | **0.10** | 398 | 384 | ✗ |
| **ML-DSA-44 (Lab 6)** | 1.46 | 0.22 | **1312** | **2420** | ✓ |

#### Key Encapsulation Mechanism

| Scheme | Encaps (ms) | Decaps (ms) | Pubkey (B) | CT size (B) | Quantum safe? |
|--------|------------|------------|-----------|-----------|--------------|
| RSA-3072 (Lab 3) | 0.20 | 8.30 | 398 | 384 | ✗ |
| **ML-KEM-512 (Lab 6)** | **0.07** | **0.12** | **800** | **768** | ✓ |

### 7.5 Phân tích

**ML-DSA vs ECDSA:**
- Sign: ML-DSA chậm hơn ~13× (1.46ms vs 0.11ms) — chấp nhận được cho hầu hết use case.
- Sig size: ML-DSA lớn hơn **~34×** (2420B vs 72B) — đây là chi phí chính của lattice signature.
- Tuy nhiên: ML-DSA **không có rủi ro nonce** (ECDSA có thể leak private key nếu nonce kém), và **kháng quantum computer**.

**ML-KEM vs RSA-3072:**
- Decaps: ML-KEM nhanh hơn **~70×** (0.12ms vs 8.3ms) — lợi thế rõ ràng cho server.
- CT size: ML-KEM lớn hơn 2× (768B vs 384B) — chi phí hợp lý cho quantum safety.
- **Kết luận:** Chuyển sang PQC không đồng nghĩa với chậm hơn — ML-KEM thậm chí nhanh hơn RSA đáng kể.

---

## 8. Security Analysis

### 8.1 Tại sao cần PQC?

Máy tính lượng tử với **Shor's algorithm** phá được trong thời gian đa thức:
- **RSA/DSA/ECDSA/ECDH**: dựa trên bài toán factorization / discrete logarithm
- **DH key exchange**: discrete logarithm

ML-DSA và ML-KEM dựa trên bài toán **Module Learning With Errors (MLWE)** — hiện chưa có thuật toán lượng tử nào (kể cả Grover's search) giải được trong thời gian đa thức.

### 8.2 Signature size vs security trade-off

ML-DSA-44 signature = **2420 bytes** vs ECDSA-P256 ~72 bytes (~34× lớn hơn).

Nguyên nhân kiến trúc:
- ECDSA signature = `(r, s)` mỗi bên 32 bytes → tổng ~64 bytes
- ML-DSA signature = hint vector `h` + response vector `z` + commitment hash `c̃`
  - `z` là vector polynomial qua module lattice — kích thước tỷ lệ với bậc của lattice
  - FIPS 204 Level 2: `(k, ℓ) = (4, 4)`, `η=2`, `β=78`, `ω=80` → sig = 2420 bytes

Đây là **chi phí cơ bản không thể tránh** nếu muốn quantum security ở NIST Level 2.

### 8.3 Hedged signing và rủi ro nonce

| Scheme | Nonce/randomness | Rủi ro nếu nonce kém |
|--------|-----------------|---------------------|
| ECDSA random k | Random mỗi lần ký | k bị reuse → **leak toàn bộ private key** |
| RFC 6979 ECDSA (Lab 5) | Deterministic HMAC-k | An toàn hơn nhưng vẫn cần implement đúng |
| **ML-DSA hedged** | Fresh randomness + message + key | **Không có rủi ro nonce leak** |

ML-DSA FIPS 204 §5.2 dùng "hedged" variant: kết hợp randomness (`ρ'`) với message và private key trong quá trình commit. Kể cả khi PRNG bị yếu, mỗi chữ ký vẫn unique và không leak private key.

### 8.4 Rejection sampling và timing

ML-DSA signing có thể **retry nội bộ** nếu response vector `z` vượt bound `β`:

```
repeat:
    y ← Sample(γ₁, γ₂)
    w = A·y mod q
    c̃ = H(μ, w₁)
    z = y + c·s₁
    if ||z||∞ ≥ γ₁ − β: restart   ← rejection sampling
    if ||h||₀ ≥ ω: restart
until success
```

Hậu quả: thời gian ký **biến thiên nhẹ** (trung bình ~4–5 lần sample). OpenSSL xử lý loop này internally. Trong production cần verify constant-time ở assembly level.

### 8.5 ML-KEM và Fujisaki–Okamoto Transform

ML-KEM-512 đạt **IND-CCA security** nhờ FO transform:

```
IND-CPA PKE (K-PKE — vulnerable to adaptive ciphertext attacks)
    ──[Fujisaki-Okamoto transform]──►
IND-CCA KEM (ML-KEM — secure against adaptive ciphertext attacks)
```

FO transform thêm **implicit rejection**: khi decapsulate ciphertext sai:
1. Re-encrypt ciphertext sai với derived key
2. So sánh re-encrypted với ciphertext gốc
3. Nếu không khớp → derive pseudo-random ss từ `Hash(ct, rejection_key)`
4. **Không bao giờ báo lỗi** — tránh timing/decryption oracle

Trong implementation:
```cpp
// Không check return value — dù ct sai vẫn trả 32 bytes
EVP_PKEY_decapsulate(ctx, ss.data(), &sslen, ct.data(), ct.size());
```

### 8.6 Tại sao ML-KEM KHÔNG dùng để ký

ML-KEM là **Key Encapsulation Mechanism** — chỉ dùng để trao đổi session key. Không có các tính chất cần thiết cho chữ ký số:

- **Non-repudiation**: không chứng minh được ai là người gửi
- **Authentication of sender**: encaps không cần private key của sender
- **Binding to identity**: shared secret không ràng buộc danh tính người gửi

Để ký cần **ML-DSA** (lattice signature). Test `test_cert` xác nhận rằng `cert_issue` với ML-KEM key sẽ throw — hành vi đúng theo thiết kế.

### 8.7 PQ Certificate và hybrid migration

**Lab implementation:** CA dùng ML-DSA-44 ký `{subject, public_key, issuer}`:
- Ký field `public_key` dưới dạng base64-DER → bind cryptographically subject identity với lattice key
- Tamper bất kỳ field nào → verify fail vì signed canonical string thay đổi
- `cert_verify()` bắt exception từ `from_base64()` → trả false thay vì throw

**Lộ trình migration thực tế (TLS 1.3):**
1. **Hybrid KEM**: `X25519MLKEM768` — kết hợp ECDH + ML-KEM trong cùng một handshake
2. **Hybrid cert**: dual-signature ECDSA + ML-DSA cho chuyển tiếp
3. **Pure PQC**: khi quantum computer đủ mạnh xuất hiện, chỉ còn ML-DSA cần thiết

---

## 9. Lessons Learned

### 9.1 Bug đã gặp và sửa

**Bug 1: cert_verify không bắt exception khi tamper signature**

**Biểu hiện:** Test "tampered signature → verify fails" crash với `base64 decode failed`.

**Nguyên nhân:** Khi tamper `cert.signature[0]` bằng XOR, ký tự mới có thể không phải base64 hợp lệ → `from_base64()` throw `std::runtime_error` → exception propagate ra ngoài `cert_verify()`.

**Fix trong `pq_cert.cpp:62`:**
```cpp
bool cert_verify(const std::string& ca_pub_pem, const PqCert& cert) {
    auto data = canonical_bytes(cert);
    std::vector<uint8_t> sig;
    try { sig = from_base64(cert.signature); }
    catch (...) { return false; } // invalid base64 → invalid cert
    return mldsa_verify(ca_pub_pem, data, sig);
}
```

**Bug 2: NULL digest trong EVP_DigestSignInit**

**Biểu hiện:** Ban đầu truyền `EVP_sha256()` → OpenSSL tạo ra Hash-ML-DSA (pre-hash variant) thay vì pure ML-DSA.

**Fix:** Truyền `nullptr` cho MD parameter → OpenSSL nhận biết pure ML-DSA-44 (FIPS 204 §5).

### 9.2 Điều rút ra

1. **OpenSSL 3.5+ đã có ML-DSA/ML-KEM native**: không cần liboqs — giảm attack surface, API chuẩn EVP.
2. **Implicit rejection là tính năng, không phải bug**: decaps không throw khi ct sai là thiết kế có chủ đích (IND-CCA). Tuyệt đối không throw sớm khi ciphertext không khớp.
3. **ML-KEM nhanh hơn RSA-3072 nhiều**: decaps 0.12ms vs RSA decrypt 8.3ms (~70×). Chuyển PQC không đồng nghĩa với chậm hơn.
4. **Kích thước là challenge chính**: 2420B signature sẽ là vấn đề với IoT, embedded, hoặc protocol có size constraints nghiêm ngặt (DNS record, CT log).
5. **PQC + classical hybrid là chuẩn tạm thời**: bảo đảm security ngay cả khi lattice bị break hoặc quantum computer xuất hiện.

---

## 10. Conclusion

Lab 6 đã xây dựng thành công `pqtool` với đầy đủ chức năng:

- **ML-DSA-44 (FIPS 204)**: keygen/sign/verify hoàn chỉnh, hedged signing, sig=2420B, pubkey=1312B
- **ML-KEM-512 (FIPS 203)**: keygen/encaps/decaps với implicit rejection đúng chuẩn, ct=768B, ss=32B
- **PQ Certificate JSON**: issue/verify với tamper detection đầy đủ (subject, public_key, issuer, signature)
- **44/44 tests PASS** (70 assertions) bao gồm negative cases, batch verification, JSON roundtrip
- **Benchmark** so sánh trực tiếp với Lab 5 (classical) và Lab 3 (RSA-KEM)

**Tổng kết so sánh PQC vs Classical:**

| Tiêu chí | Classical | PQC (Lab 6) | Nhận xét |
|---------|-----------|-----------|---------|
| Sign speed | ECDSA: 0.11 ms | ML-DSA-44: 1.46 ms | ~13× chậm hơn — chấp nhận được |
| Verify speed | ECDSA: 0.19 ms | ML-DSA-44: 0.22 ms | Tương đương |
| Signature size | ECDSA: 72 B | ML-DSA-44: 2420 B | ~34× lớn hơn — chi phí chính |
| KEM speed (decaps) | RSA: 8.3 ms | ML-KEM: 0.12 ms | **70× nhanh hơn** |
| Quantum safe | ✗ | ✓ | Mục tiêu chính |

---

## 11. References

1. **FIPS 203** — "Module-Lattice-Based Key-Encapsulation Mechanism Standard", NIST, August 2024.
2. **FIPS 204** — "Module-Lattice-Based Digital Signature Standard", NIST, August 2024.
3. **CRYSTALS-Kyber** — Bos, J. et al. "CRYSTALS–Kyber: a CCA-secure module-lattice-based KEM." EuroS&P, 2018.
4. **CRYSTALS-Dilithium** — Ducas, L. et al. "CRYSTALS-Dilithium: A Lattice-Based Digital Signature Scheme." IACR TCHES, 2018.
5. **OpenSSL 3.6.2 Documentation** — `EVP_PKEY_encapsulate(3)`, `EVP_DigestSign(3)`, `EVP_PKEY_keygen(3)`.
6. **Fujisaki-Okamoto Transform** — Fujisaki, E. & Okamoto, T. Journal of Cryptology, 26(1):80–101, 2013.
7. **NIST PQC Standardization Project** — Final Standards overview. https://csrc.nist.gov/pqc
8. **Grover's Algorithm** — Grover, L. K. "A fast quantum mechanical algorithm for database search." STOC'96.
9. **RFC 9629** — "Using KEM Algorithms in CMS", IETF, 2024.
10. **nlohmann/json** — Lohmann, N. JSON for Modern C++ v3.11.3.
11. **Bernstein, D. J. & Lange, T.** — "Post-Quantum Cryptography: State of Play." Nature, 549:188–194, 2017.
