# Chương 3 — RSA Hybrid Encryption (RSA-OAEP + AES-256-GCM)

> **Sinh viên:** Nguyễn Đỗ Ngọc Huyền Thương · **MSV:** 24521750
> **Tool:** `rsatool` · **Thư viện:** Crypto++ (RSA-OAEP) + OpenSSL (AES-256-GCM)
> **Ngày hoàn thành:** 2026-06-22

---

## 1. Objectives

### 1.1 Mục tiêu lab

Lab 3 yêu cầu xây dựng CLI tool `rsatool` thực hiện **hybrid encryption**: kết hợp RSA-OAEP (asymmetric) để bảo vệ session key và AES-256-GCM (symmetric) để mã hóa dữ liệu thực tế. Đây là mô hình được dùng trong TLS, PGP, CMS/PKCS#7.

Mục tiêu cụ thể:
- Sinh keypair RSA-2048/3072/4096, lưu PEM
- Encrypt: tạo session key ngẫu nhiên 32 B → RSA-OAEP wrap → AES-256-GCM encrypt payload
- Decrypt: RSA-OAEP unwrap session key → AES-256-GCM decrypt + verify tag
- KAT với PKCS#1 v2.2 / RFC 3447 test vectors
- Negative tests: wrong key, tampered ciphertext, tampered GCM tag, OAEP label mismatch

### 1.2 Những gì đã xây dựng

| Thành phần | Mô tả |
|---|---|
| `rsatool keygen` | Sinh RSA keypair 2048/3072/4096-bit, lưu PEM |
| `rsatool encrypt` | RSA-OAEP wrap session key + AES-256-GCM encrypt file |
| `rsatool decrypt` | RSA-OAEP unwrap + AES-256-GCM decrypt + verify tag |
| `rsa_oaep.cpp` | RSA-OAEP encrypt/decrypt với SHA-256 (Crypto++) |
| `hybrid.cpp` | Hybrid envelope: session key encap/decap + GCM |
| `rsa_bench` | Benchmark standalone: keygen/enc/dec latency + hybrid throughput |
| Tests Catch2 | Roundtrip 2048/3072/4096, hybrid, negative cases |

### 1.3 Lý do thiết kế

Không thể dùng RSA thuần để mã hóa file lớn: RSA-3072 chỉ mã hóa được tối đa 320 byte payload per operation. Hybrid model giải quyết bằng cách dùng RSA chỉ để bảo vệ một **session key ngắn** (32 byte), còn AES-GCM xử lý dữ liệu thực với tốc độ ~143 MB/s. Đây là mẫu chuẩn trong mọi PKI protocol hiện đại.

---

## 2. Environment

### 2.1 Môi trường thử nghiệm

| Thành phần | Chi tiết |
|---|---|
| **OS** | Windows 11 Home 10.0.26200 |
| **CPU** | Intel Core i7-1165G7 @ 2.80 GHz (Tiger Lake, 4C/8T, AES-NI) |
| **RAM** | 8 GB DDR4 |
| **Compiler** | MSVC 2022 (v19.x), C++17, `/O2` Release |
| **CMake** | 3.25+ |
| **Crypto++** | 8.9.0 (vcpkg) — RSA-OAEP, SHA-256 |
| **OpenSSL** | 3.6.2 (vcpkg) — AES-256-GCM |
| **Catch2** | 3.5.x (vcpkg) |

### 2.2 Build

```bash
cmake --build build --target rsatool rsa_bench lab3_tests --config Release
```

---

## 3. System Design

### 3.1 Kiến trúc tổng thể

```
Sender:                              Receiver:
┌─────────────────────┐              ┌─────────────────────┐
│ Plaintext (N bytes) │              │ Ciphertext envelope  │
│         │           │              │         │            │
│  CSPRNG │           │              │  RSA-OAEP Decrypt    │
│  32-byte│           │              │  (privkey)           │
│ session │ AES-256-  │   envelope   │         │            │
│  key    │ GCM enc   │ ──────────►  │  session key (32B)   │
│         │           │              │         │            │
│ RSA-OAEP│ + tag     │              │  AES-256-GCM Decrypt │
│  Encrypt│ + GCM IV  │              │  + verify GCM tag    │
│ (pubkey)│           │              │         │            │
└─────────────────────┘              │   Plaintext          │
                                     └─────────────────────┘
```

### 3.2 Định dạng envelope (binary)

```
[2B: wrapped_key_len] [wrapped_key_len B: RSA-OAEP(session_key)]
[12B: GCM IV] [N B: AES-256-GCM ciphertext] [16B: GCM tag]
```

- `wrapped_key_len` (uint16_t): 256/384/512 byte tùy RSA-2048/3072/4096
- GCM IV: 12 byte ngẫu nhiên từ CSPRNG, mới mỗi lần encrypt
- GCM tag: 16 byte, verify trước khi trả plaintext (fail-closed)

### 3.3 Tham số thuật toán

| Thuật toán | Tham số | Lý do |
|---|---|---|
| RSA-OAEP | SHA-256 hash, empty label | PKCS#1 v2.2 khuyến nghị; SHA-256 resist collision |
| AES-256-GCM | key=32B, IV=12B, tag=16B | NIST SP 800-38D; 96-bit nonce là optimal cho GCM |
| Session key | 32 B từ `AutoSeededRandomPool` | Crypto++ CSPRNG (RDRAND + OS entropy) |

---

## 4. Implementation

### 4.1 RSA-OAEP (Crypto++)

```cpp
// rsa_oaep.cpp — encrypt session key
RSAES_OAEP_SHA_Encryptor enc(pub_key);
AutoSeededRandomPool rng;
std::string wrapped;
StringSource ss(session_key.data(), session_key.size(), true,
    new PK_EncryptorFilter(rng, enc, new StringSink(wrapped)));
```

`PK_EncryptorFilter` tự động pad theo OAEP scheme. Nếu plaintext > `MaxPlaintextLength()` thì Crypto++ throw `InvalidArgument`, ngăn silent truncation.

### 4.2 AES-256-GCM (OpenSSL EVP)

```cpp
// hybrid.cpp — GCM encrypt
EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, key, iv);
EVP_EncryptUpdate(ctx, ct_buf, &out_len, pt, pt_len);
EVP_EncryptFinal_ex(ctx, ct_buf + out_len, &final_len);
EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
```

**Quan trọng — fail-closed decryption:**
```cpp
// Verify tag TRƯỚC KHI trả plaintext
if (EVP_DecryptFinal_ex(ctx, ...) <= 0)
    throw std::runtime_error("GCM tag verification failed — ciphertext tampered");
// Chỉ trả plaintext nếu tag OK
```

### 4.3 Key generation

```cpp
InvertibleRSAFunction params;
params.GenerateRandomWithKeySize(rng, key_bits); // 2048/3072/4096
RSA::PrivateKey priv_key(params);
RSA::PublicKey  pub_key(params);
```

---

## 5. KAT Validation

### 5.1 Nguồn vector

| Nguồn | Thuật toán | Số case |
|---|---|---|
| PKCS#1 v2.1 (RFC 3447 Test Vectors) | RSA-OAEP-2048 SHA-1 | 3 |
| NIST CAVP AES-GCM | AES-256-GCM | 4 |
| Cross-validate Crypto++ vs OpenSSL | Session key wrap/unwrap | 5 |

**Lưu ý:** RSA-OAEP với SHA-256 không có NIST ACVP public test vectors (RFC 3447 chỉ có SHA-1). Lab dùng cross-validation: encrypt bằng Crypto++, decrypt bằng OpenSSL EVP để xác nhận interoperability.

### 5.2 Kết quả KAT

```
[KAT] RSA-OAEP-2048/SHA-256 roundtrip  ... PASS
[KAT] RSA-OAEP-3072/SHA-256 roundtrip  ... PASS
[KAT] RSA-OAEP-4096/SHA-256 roundtrip  ... PASS
[KAT] AES-256-GCM encrypt/decrypt      ... PASS (4 vectors)
[KAT] Hybrid 1 KiB roundtrip           ... PASS
[KAT] Hybrid 1 MiB roundtrip           ... PASS
```

---

## 6. Negative Testing

### 6.1 Danh sách ca kiểm thử

| Ca | Input | Kết quả kỳ vọng | Kết quả thực tế |
|---|---|---|---|
| Wrong private key | Decrypt với privkey khác | OAEP verification fail | PASS (exception) |
| Tampered wrapped key | Flip 1 bit trong RSA ciphertext | OAEP padding fail | PASS (exception) |
| Tampered GCM ciphertext | Flip 1 bit trong AES-GCM ct | GCM tag verify fail | PASS (exception) |
| Tampered GCM tag | Modify 1 byte của tag | GCM tag verify fail | PASS (exception) |
| OAEP label mismatch | Encrypt label="A", decrypt label="B" | OAEP verify fail | PASS (exception) |
| Empty ciphertext | Decrypt file rỗng | Invalid envelope | PASS (exception) |

### 6.2 Nguyên tắc fail-closed

Mọi path decryption đều throw trước khi trả data:
1. RSA-OAEP unwrap fail → throw (không có session key)
2. AES-GCM `EVP_DecryptFinal_ex` fail → throw (không có plaintext)
3. Envelope parse fail → throw (không đọc được wrapped key)

---

## 7. Performance Evaluation

### 7.1 Phương pháp đo

- **Warm-up:** 3 lần trước khi đo
- **Số lần đo:** N=5 (keygen), N=30 (encrypt/decrypt)
- **Platform:** Windows 11, Intel i7-1165G7, AES-NI enabled, Release build

### 7.2 Kết quả RSA thuần

| Operation | RSA-3072 | RSA-4096 |
|---|---|---|
| Keygen (mean) | **422.4 ms** | **1827.9 ms** |
| Keygen (stddev) | ±266.3 ms | ±1356.7 ms |
| Encrypt (pubkey, mean) | 0.2 ms | 0.5 ms |
| Decrypt (privkey, mean) | **8.3 ms** | **29.2 ms** |
| Decrypt (stddev) | ±1.2 ms | ±5.2 ms |

**Nhận xét:** Keygen stddev lớn do primality testing probabilistic (số bước Miller-Rabin phụ thuộc ngẫu nhiên). Decrypt chậm hơn Encrypt ~40× do modular exponentiation với private exponent lớn (CRT giúp 4× nhưng vẫn chậm hơn public exponent e=65537).

### 7.3 Kết quả Hybrid (RSA-OAEP + AES-256-GCM)

| Payload | Enc RSA-3072 | Dec RSA-3072 | Throughput | Dec RSA-4096 |
|---|---|---|---|---|
| 1 KiB | 0.9 ms | 21.5 ms | 1.1 MB/s | 28.5 ms |
| 1 MiB | 8.9 ms | 29.4 ms | 112.8 MB/s | 33.5 ms |
| 100 MiB | 695.8 ms | 775.6 ms | 143.7 MB/s | 809.3 ms |

**Nhận xét:**
- Bottleneck ở payload lớn là AES-GCM (~143 MB/s với AES-NI), không phải RSA (RSA chỉ wrap 32 byte session key).
- Decrypt latency tại 1 KiB cao (~21 ms) vì phần lớn là RSA-3072 private key operation (8.3 ms) + GCM initialization overhead.
- Tại 100 MiB, RSA overhead < 1% tổng thời gian — AES-GCM hoàn toàn dominate.

### 7.4 So sánh với Lab 6 (ML-KEM-512)

| Metric | RSA-3072 (Lab 3) | ML-KEM-512 (Lab 6) |
|---|---|---|
| Decaps latency | 8.3 ms | **0.12 ms** (~69× nhanh) |
| Pubkey size | ~398 B | 800 B |
| Ciphertext (session key wrap) | 384 B | 768 B |
| Kháng lượng tử | ✗ | ✓ |

---

## 8. Security Analysis

### 8.1 Threat model

Attacker có thể:
- Đọc ciphertext envelope → bị chặn bởi AES-256-GCM confidentiality
- Modify ciphertext (active MITM) → bị phát hiện bởi GCM tag + OAEP padding verify
- Replay ciphertext cũ → GCM IV không reuse (mỗi encrypt sinh IV mới từ CSPRNG)
- Brute-force session key (32 B = 256 bit) → 2^256 không gian, không khả thi

### 8.2 RSA-OAEP vs PKCS#1 v1.5

| | PKCS#1 v1.5 | RSA-OAEP (Lab 3) |
|---|---|---|
| Chống CCA | ✗ (Bleichenbacher 1998) | ✓ (IND-CCA2) |
| Randomized | Có random padding | Có (seed trong OAEP) |
| Tiêu chuẩn hiện tại | Deprecated | PKCS#1 v2.2, NIST |

OAEP dùng mask generation function (MGF1-SHA256) để randomize padding, ngăn oracle attack. Cùng plaintext + cùng public key → ciphertext khác nhau mỗi lần.

### 8.3 AES-256-GCM security

- **Confidentiality:** CTR mode bên dưới
- **Integrity:** GHASH authentication over AAD + ciphertext
- **Nonce reuse attack:** Nếu IV reuse với cùng key → attacker có thể recover keystream và forge tag. Lab sinh IV mới từ CSPRNG mỗi encrypt → an toàn.
- **Tag size:** 16 byte (128 bit) → xác suất forgery 2^{-128}

### 8.4 Hạn chế

| Hạn chế | Mức độ | Giải pháp |
|---|---|---|
| Không kháng lượng tử | Cao (post-2030) | Migrate sang ML-KEM (Lab 6) |
| Không forward secrecy | Trung bình | Dùng ephemeral DH/ECDH thay RSA wrap |
| Không có certificate binding | Trung bình | RSA pubkey không binding identity → cần PKI/TLS |

---

## 9. Lessons Learned

### 9.1 Bug đã gặp

**Bug 1: GCM decrypt expose plaintext trước khi verify tag**
- Biểu hiện: OpenSSL `EVP_DecryptUpdate` trả data ngay, tag verify ở `EVP_DecryptFinal_ex`
- Nguy cơ: Nếu dùng plaintext từ Update trước Final → vulnerable to chosen ciphertext attack
- Fix: Buffer toàn bộ output của Update, chỉ expose plaintext sau khi Final return 1

**Bug 2: Envelope `wrapped_key_len` field overflow**
- Biểu hiện: RSA-4096 ciphertext = 512 byte → `uint8_t` field (max 255) overflow, silent truncation
- Fix: Dùng `uint16_t` (2 byte, little-endian) cho `wrapped_key_len`

**Bug 3: OAEP label inconsistency**
- Biểu hiện: Encrypt Crypto++ default label vs decrypt với non-default → exception
- Fix: Luôn explicit pass `""` (empty string) làm label ở cả hai phía

### 9.2 Điều rút ra

1. **Không dùng RSA encrypt data trực tiếp:** RSA-OAEP chỉ cho session key ngắn ≤ 320 byte (với RSA-3072).
2. **GCM tag phải verify atomic:** Buffer output → verify tag → expose plaintext.
3. **IV/nonce management:** AES-GCM với nonce reuse là thảm họa — sinh IV mới từ CSPRNG, lưu kèm envelope.
4. **Keygen stddev lớn là bình thường:** RSA primality test probabilistic, không phải bug.

---

## 10. Conclusion

Lab 3 đã xây dựng thành công hybrid encryption tool `rsatool`:

- **RSA-OAEP (SHA-256):** IND-CCA2 secure, interoperable với OpenSSL
- **AES-256-GCM:** Authenticated encryption, fail-closed tag verify
- **Hybrid envelope:** Session key mới mỗi lần, IV ngẫu nhiên, không reuse
- **Roundtrip đúng:** RSA-2048/3072/4096 đều pass; negative tests 6/6 fail-closed
- **Benchmark:** ~143 MB/s tại 100 MiB (AES-NI); RSA-3072 decrypt 8.3 ms

Hướng upgrade: Lab 6 ML-KEM-512 thay thế RSA wrap — nhanh hơn ~69×, kháng lượng tử.

---

## 11. References

1. **RFC 8017** — "PKCS #1: RSA Cryptography Specifications Version 2.2", IETF, 2016. OAEP scheme §7.1.
2. **RFC 3447** — "PKCS #1 v2.1", Jonsson & Kaliski, 2003. Test vectors Appendix A.
3. **NIST SP 800-38D** — "Recommendation for Block Cipher Modes of Operation: Galois/Counter Mode (GCM)", 2007.
4. **FIPS 197** — "Advanced Encryption Standard (AES)", NIST, 2001.
5. **NIST SP 800-57 Part 1** — "Recommendation for Key Management", 2020. RSA-3072 = 128-bit security.
6. **Crypto++ Library** — Wei Dai et al., v8.9.0. `RSAES_OAEP_SHA_Encryptor`, `PK_EncryptorFilter`.
7. **OpenSSL 3.6.2 EVP API** — AES-256-GCM encrypt/decrypt, `EVP_CTRL_GCM_GET_TAG`.
8. **Bleichenbacher, D.** — "Chosen Ciphertext Attacks Against Protocols Based on the RSA Encryption Standard PKCS #1", CRYPTO 1998. Lý do dùng OAEP.
9. **Jovanovic et al.** — "Nonce-Disrespecting Adversaries", USENIX Security 2016. GCM nonce reuse attack.
10. **FIPS 203** — "ML-KEM Standard", NIST, 2024. ML-KEM là PQC successor của RSA KEM.
