# Lab 6 — Post-Quantum Signatures & Certificates (ML-DSA, ML-KEM)

> **Tool:** `pqtool` · **Thư viện:** OpenSSL **3.5+** (native ML-DSA/ML-KEM) hoặc **liboqs/OQS-provider** · **Rubric:** 100 (+15 bonus)
> Lab cuối: mật mã **hậu lượng tử** (lattice-based) + mini-project chứng chỉ PQ. So sánh với Lab 5.

---

## 1. Lab Overview

Xây `pqtool`: keygen/sign/verify với **ML-DSA-44** (FIPS 204) và encaps/decaps với **ML-KEM-512** (FIPS 203), cộng một **PQ certificate mini-project** (CA ký public key bằng ML-DSA). ML-DSA thay thế chữ ký cổ điển; ML-KEM thay thế RSA key exchange.

**Learning Outcomes (đề):** sinh keypair PQC; ký/verify ML-DSA detached; encaps/decaps ML-KEM; hiểu tham số & security level; benchmark; xây cert PQ.

> 🔗 **So sánh bắt buộc với [Lab 5](lab5_signatures.md):** kích thước chữ ký, tốc độ sign/verify, key size. 🔗 ML-KEM là "upgrade path" của hybrid RSA ở [Lab 3](lab3_rsa_hybrid.md).
>
> 💡 **Thư viện:** Kiểm tra `openssl list -signature-algorithms | findstr -i ml-dsa`. Nếu OpenSSL < 3.5 → cài **liboqs** (`vcpkg install liboqs`) + dùng API OQS. Đề chấp nhận "theoretical comparison" cho một số phần.

---

## 2. Requirement Breakdown

### A. Chức năng
**Bắt buộc:**
- [ ] `keygen --algo mldsa-44 | mlkem-512` → PEM/DER
- [ ] `sign --algo mldsa-44 --in --out`; `verify --algo mldsa-44 --in --sig --pub`
- [ ] `encaps --algo mlkem-512 --pub --ct --ss`; `decaps --priv --ct --ss`
- [ ] Signature raw/base64; ciphertext binary; (optional) JSON certificate
- [ ] **PQ certificate**: `{subject, public_key, issuer, signature}` — CA(ML-DSA) ký subject pubkey, verify, detect tampering

**Bonus:** ML-DSA-65 (+5); formula-level ML-DSA/ML-KEM (+15).

### B. Bảo mật
- [ ] Decaps xử lý lỗi đúng (FO transform: implicit rejection — luôn trả shared secret, không leak)
- [ ] RNG an toàn cho keygen/encaps

### C. Testing
- [ ] Negative: modified message/signature/pubkey → verify fail; modified KEM ciphertext → **decaps fail/mismatch**; wrong priv key → fail
- [ ] Unit test; **batch verification** (verify N); **batch decapsulation timing**

### D. Benchmark
- [ ] ML-DSA: keygen/sign/verify latency + ops/sec
- [ ] ML-KEM: keygen/encaps/decaps latency + shared-secret throughput
- [ ] msg sizes 1KiB/16KiB/1MiB/8MiB; so sánh ECDSA/RSA-PSS (theoretical OK)

### E. Báo cáo
- [ ] Signature size vs security; **chữ ký lớn** vs ECDSA; deterministic signing (không lỗi RFC6979); rejection sampling & failure probability; IND-CCA; **Fujisaki–Okamoto transform**; ciphertext size vs RSA; vì sao ML-KEM **không** dùng để ký; hybrid cert; migration PQ TLS

---

## 3. Folder Structure

```
lab6_post_quantum/
├── CMakeLists.txt
├── include/
│   ├── mldsa.hpp        # keygen/sign/verify
│   ├── mlkem.hpp        # keygen/encaps/decaps
│   ├── pq_cert.hpp      # cert mini-project (JSON)
│   └── key_store.hpp    # PEM/DER
├── src/ (+ main.cpp)
├── tests/
│   ├── test_mldsa.cpp
│   ├── test_mlkem.cpp
│   ├── test_cert.cpp
│   └── test_negative.cpp
├── kat/
│   ├── mldsa44_kat.json    # NIST FIPS 204 ACVP vectors
│   └── mlkem512_kat.json   # NIST FIPS 203 ACVP vectors
└── benchmark/bench_main.cpp
```

---

## 4. Architecture

```
                    ┌──────── pqtool (CLI) ────────┐
 keygen --algo ───► │ KeyStore.gen → PEM/DER       │
 sign / verify ───► │ MlDsa (FIPS 204)             │── sig (raw/base64), PASS/FAIL
 encaps/decaps ───► │ MlKem (FIPS 203)             │── ciphertext + shared secret
                    │ PqCert: CA sign pubkey       │── cert.json, verify/tamper
                    └──────────────────────────────┘
        Backend: OpenSSL 3.5 EVP_PKEY  |  liboqs OQS_SIG / OQS_KEM
```
- **MlDsa/MlKem:** bọc API thư viện (EVP hoặc OQS) sau interface gọn.
- **PqCert:** JSON `{subject, public_key, issuer, signature}` + verify chữ ký CA.

---

## 5. Classes

| Class | Chức năng | Public methods | Private |
|-------|-----------|----------------|---------|
| `MlDsa` | ML-DSA-44/65 | `keygen()`, `sign(priv,msg)`, `verify(pub,msg,sig)` | param set |
| `MlKem` | ML-KEM-512 | `keygen()`, `encaps(pub)→{ct,ss}`, `decaps(priv,ct)→ss` | param set |
| `PqCert` | cert mini-project | `issue(caPriv, subject, subjPub)`, `verify(caPub, cert)` | JSON |
| `KeyStore` | PEM/DER | `save/load` | — |
| `BenchmarkRunner` (common) | ops/sec + batch | `measure(...)` | — |

---

## 6. Implementation Roadmap

| Step | Việc | "Done" khi |
|------|------|-----------|
| 1 | Setup + backend (OpenSSL3.5 / liboqs); xác minh thuật toán sẵn có | `pqtool --help`; list algo OK |
| 2 | `keygen` ML-DSA-44 + ML-KEM-512 PEM/DER | load lại OK |
| 3 | `MlDsa.sign/verify` detached | round-trip + KAT PASS |
| 4 | `MlKem.encaps/decaps` | shared secret 2 bên **khớp** |
| 5 | `PqCert.issue/verify` + tamper detect | sửa cert → verify FAIL |
| 6 | Negative tests + batch verify + batch decaps timing | đủ ca, fail closed |
| 7 | Benchmark ML-DSA/ML-KEM + bảng so Lab 5 | plot + bảng |
| 8 | (Bonus) ML-DSA-65 / formula-level | KAT PASS |

---

## 7. Testing Plan

| Test | Mục tiêu | Input | Expected |
|------|----------|-------|----------|
| ML-DSA round-trip | sign/verify | msg, key | PASS |
| ML-DSA KAT | đúng FIPS 204 | ACVP vector | khớp |
| ML-KEM correctness | ss khớp | encaps→decaps | `ss_A == ss_B` |
| ML-KEM KAT | đúng FIPS 203 | ACVP vector | khớp |
| Cert verify | CA chữ ký | cert hợp lệ | PASS; sửa field → FAIL |
| Modified msg/sig/pub | integrity | sửa từng phần | verify FAIL |
| Modified KEM ct | — | lật bit ct | decaps **mismatch** (implicit reject) |
| Wrong priv key | — | priv khác | fail/mismatch |
| Batch | hiệu năng | N=1000 | đếm đúng + đo timing |

**KAT nguồn:** NIST **ACVP** cho FIPS 203 (ML-KEM) & FIPS 204 (ML-DSA).

---

## 8. Benchmark Plan

- **ML-DSA:** keygen/sign/verify latency + ops/sec.
- **ML-KEM:** keygen/encaps/decaps latency + shared-secret throughput; **batch decaps timing** (kiểm tra input-dependent timing).
- **Sizes:** 1KiB/16KiB/1MiB/8MiB (chữ ký detached → chi phí chủ yếu là hash msg + thao tác lattice cố định).
- **Bảng so sánh (đề yêu cầu) — điền số thật từ Lab 5 + Lab 6:**

  | Scheme | Sign | Verify | Pubkey | Signature |
  |--------|------|--------|--------|-----------|
  | ECDSA-P256 (Lab 5) | nhanh | nhanh | 32–64 B | ~64–72 B |
  | RSA-PSS-3072 (Lab 5) | chậm | rất nhanh | ~398 B | 384 B |
  | **ML-DSA-44** | TB | TB | ~1312 B | **~2420 B** |

  | KEM | Encaps | Decaps | Pubkey | Ciphertext |
  |-----|--------|--------|--------|------------|
  | RSA-3072 (Lab 3 wrap) | nhanh | chậm | ~398 B | 384 B |
  | **ML-KEM-512** | nhanh | nhanh | ~800 B | ~768 B |

- **Nhận xét:** PQ đổi **kích thước lớn** lấy **kháng lượng tử**; ML-KEM nhanh & cân bằng hơn RSA.

---

## 9. Security Analysis

**Bắt buộc bàn (đề):**
- **Signature size vs security:** ML-DSA chữ ký **lớn hơn nhiều** ECDSA (~2.4KB vs ~64B) — chi phí của lattice.
- **Deterministic signing:** ML-DSA (mặc định hedged/deterministic) **không** dính lỗi nonce kiểu RFC6979 của ECDSA.
- **Rejection sampling & failure probability:** ML-DSA loại bỏ chữ ký rò rỉ thông tin → thời gian ký **biến thiên** (bàn về constant-time).
- **IND-CCA (ML-KEM):** đạt qua **Fujisaki–Okamoto transform** (IND-CPA PKE → IND-CCA KEM) với **implicit rejection** (decaps lỗi vẫn trả ss giả, không leak).
- **Ciphertext size vs RSA:** ML-KEM-512 ct ~768B; cân bằng hiệu năng tốt.
- **Vì sao ML-KEM KHÔNG dùng để ký:** KEM là cơ chế trao đổi khóa (encaps/decaps), **không** cung cấp tính không chối bỏ/xác thực nguồn gốc — ký phải dùng **ML-DSA**.
- **Hybrid cert & migration:** chứng chỉ **ECDSA + ML-DSA** (dual signature) để chuyển tiếp an toàn; lộ trình **PQ TLS**.

**Bonus +15:** NTT polynomial arithmetic, modular reduction, rejection sampling, constant-time NTT; timing attack surface, lattice leakage, failure handling trong decaps; basic timing variance + input-dependent timing discussion.

**Limitations:** dựa thư viện (constant-time do lib); KAT theo ACVP; cert mini-project là cấu trúc giản lược (không phải X.509 đầy đủ).

---

## 10. Report Template (mục Lab 6 + so sánh tổng)

Theo [11 mục](00_report_template.md#2-mẫu-11-mục-cho-mỗi-lab-lặp-lại-cho-lab-16). Riêng Lab 6:
- **System Design:** cấu trúc PQ cert JSON; backend (OpenSSL3.5/liboqs).
- **Performance:** 2 bảng so sánh ở §8 (điền số thật) + plot.
- **Security:** FO transform diagram; vì sao chữ ký lớn; ML-KEM ≠ signing.
- **Cross-lab (PHẦN 7 báo cáo tổng):** so Lab 5 (classical) vs Lab 6 (PQC) — size/speed/migration.
- **Hình:** encaps/decaps ss khớp; cert verify PASS + tamper FAIL.

---

## 11. Common Mistakes

- ❌ Dùng OpenSSL < 3.5 mà không cài liboqs → không có ML-DSA/ML-KEM.
- ❌ Nhầm **ML-KEM dùng để ký** (sai bản chất — KEM không ký).
- ❌ Decaps fail rồi **báo lỗi leak** thay vì implicit rejection (trả ss giả).
- ❌ So sánh ss bằng `==` không constant-time (nên `CRYPTO_memcmp`).
- ❌ Lưu signature/ciphertext sai (sig base64/raw; ct binary).
- ❌ Quên batch verify / batch decaps timing.
- ❌ Cert: ký nhầm trường, hoặc verify không bao trùm toàn bộ `{subject,public_key,issuer}`.
- ❌ Không chừa bảng so sánh với Lab 5 (đề yêu cầu so sánh).

---

## 12. Final Submission Checklist (Lab 6)

- [ ] ML-DSA-44 sign/verify + ML-KEM-512 encaps/decaps (ss khớp)
- [ ] keygen PEM/DER; sig raw/base64; ct binary
- [ ] PQ certificate: issue/verify + tamper detection
- [ ] KAT ACVP (FIPS 203 + 204) PASS
- [ ] Negative: msg/sig/pub/ct/priv + batch verify + batch decaps timing
- [ ] Benchmark ML-DSA/ML-KEM + **bảng so sánh Lab 5**
- [ ] `ctest` pass Win+Linux; report Lab 6 + so sánh tổng
- [ ] (Bonus) ML-DSA-65 (+5) / formula-level (+15)

**Rubric Lab 6 (100 + 15):** ML-DSA **25** · ML-KEM **20** · Key handling & formats **15** · Certificate **10** · Negative & UX **10** · Performance **15** · Cross-platform & doc **5** · Report **10** · Bonus advanced **+15**.

> Quay lại [README bộ guide](README.md) · [Tổng quan](00_master_overview.md).
