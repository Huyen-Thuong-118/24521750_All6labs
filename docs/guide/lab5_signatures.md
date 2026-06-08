# Lab 5 — Classical Digital Signatures (ECDSA, RSA-PSS)

> **Tool:** `sigtool` · **Thư viện:** Crypto++ **+** OpenSSL · **Rubric:** 100 (+15 bonus)
> Lab về **chữ ký số cổ điển** — nền để so sánh với PQC (Lab 6).

---

## 1. Lab Overview

Xây `sigtool`: sinh khóa, ký & verify **detached signature** cho **ECDSA-P256** (deterministic, RFC 6979) và **RSA-PSS-3072 (SHA-256)**. Trọng tâm: chọn tham số an toàn, **nonce reuse là thảm họa của ECDSA**, và so sánh EC vs RSA.

**Learning Outcomes (đề):** sinh keypair ECDSA & RSA-PSS; ký/verify detached; chọn tham số & hash an toàn; phát hiện misuse; benchmark sign/verify; so sánh EC vs RSA.

> 🔗 Số liệu & kết luận lab này được **so sánh trực tiếp** với ML-DSA ở [Lab 6](lab6_post_quantum.md). CLI thiết kế **mirror Lab 6** để nhất quán.

---

## 2. Requirement Breakdown

### A. Chức năng
**Bắt buộc:**
- [ ] `keygen --algo ecdsa-p256 | rsa-pss-3072` → PEM/DER
- [ ] `sign --algo ... --in --out --hash sha256`; `verify --algo ... --in --sig --pub`
- [ ] ECDSA-P256 **deterministic (RFC 6979)** (mặc định); RSA-PSS-3072 SHA-256, **salt = hashLen (32B)**, salt ngẫu nhiên mỗi chữ ký
- [ ] Signature encoding: raw / DER / base64 (`--encode`)
- [ ] Validate: malformed keys, sai algo id, encoding không hỗ trợ, hash mismatch

**Nên/Bonus:** ECDSA-**P384** (+5); formula-level implementation (+15).

### B. Bảo mật
- [ ] ECDSA dùng **deterministic nonce (RFC 6979)** trừ khi có lý do chính đáng
- [ ] RSA-PSS: public exponent **65537**; randomized salt
- [ ] Verify **constant-time**; error handling **không leak** thông tin

### C. Testing
- [ ] Negative: modified message / signature / public key / wrong algo id / wrong hash → đều **fail**
- [ ] Unit test tự động; **batch verification** (verify N chữ ký); error code rõ ràng

### D. Benchmark
- [ ] ECDSA & RSA-PSS: keygen / sign / verify latency + throughput (ops/sec)
- [ ] Message sizes **1KiB / 16KiB / 1MiB / 8MiB**
- [ ] So sánh với **Lab 6 (PQC)** trong discussion

### E. Báo cáo
- [ ] RFC 6979 vs random nonce; nonce reuse catastrophe; signature size EC vs RSA; verify vs sign cost; vì sao PSS > PKCS#1 v1.5; vai trò salt; exponent 65537

---

## 3. Folder Structure

```
lab5_signatures/
├── CMakeLists.txt
├── include/
│   ├── ecdsa_signer.hpp     # P-256/P-384, RFC 6979
│   ├── rsapss_signer.hpp    # RSA-PSS-3072 SHA-256
│   ├── signer.hpp           # interface chung sign/verify
│   └── key_store.hpp        # PEM/DER
├── src/ (+ main.cpp)
├── tests/
│   ├── test_ecdsa.cpp       # + RFC 6979 KAT
│   ├── test_rsapss.cpp
│   ├── test_negative.cpp
│   └── test_batch.cpp
├── kat/
│   ├── ecdsa_rfc6979.json   # deterministic vectors
│   └── rsapss_vectors.json
└── benchmark/bench_main.cpp
```

---

## 4. Architecture

```
                    ┌──────── sigtool (CLI) ────────┐
 keygen --algo ───► │ KeyStore.gen → PEM/DER        │
                    ├───────────────────────────────┤
 sign  --in --hash► │ Signer (ECDSA | RSA-PSS)      │── sig (raw/DER/base64)
 verify --sig ────► │   .sign(priv,msg) .verify(... )│── PASS/FAIL
                    └───────────────────────────────┘
        Hash layer (SHA-256/384) ── digest ──► Signer
```
- **Signer interface** → 2 impl (`EcdsaSigner`, `RsaPssSigner`) để CLI thống nhất.
- **Detached:** chữ ký lưu file riêng, không nhúng vào message.

---

## 5. Classes

| Class | Chức năng | Public methods | Private |
|-------|-----------|----------------|---------|
| `Signer` (interface) | hợp đồng chung | `sign(priv,msg)`, `verify(pub,msg,sig)` | — |
| `EcdsaSigner` | ECDSA P-256/384 RFC6979 | như trên + `curve()` | EC params |
| `RsaPssSigner` | RSA-PSS-3072 SHA-256 | như trên + `saltLen()` | salt config |
| `KeyStore` | PEM/DER I/O | `gen(algo)`, `load/save` | — |
| `SigCodec` | raw/DER/base64 | `encode/decode` | — |
| `BenchmarkRunner` (common) | ops/sec | `measure(...)` | — |

---

## 6. Implementation Roadmap

| Step | Việc | "Done" khi |
|------|------|-----------|
| 1 | Setup + link OpenSSL(+Crypto++); `Signer` interface | `sigtool --help` |
| 2 | `keygen` ECDSA-P256 + RSA-PSS-3072 PEM/DER | load lại OK |
| 3 | `EcdsaSigner` sign/verify **RFC 6979** | KAT RFC6979 PASS (deterministic) |
| 4 | `RsaPssSigner` sign/verify (salt 32B) | round-trip OK |
| 5 | SigCodec raw/DER/base64 + `--encode` | 3 encoding interchange OK |
| 6 | Validate (algo id, hash mismatch, malformed key) | fail closed |
| 7 | Negative tests + batch verify | 5 ca fail; verify N nhanh |
| 8 | Benchmark sign/verify/keygen 4 size + ops/sec | bảng + plot |
| 9 | (Bonus) P-384 / formula-level | KAT PASS |

---

## 7. Testing Plan

| Test | Mục tiêu | Input | Expected |
|------|----------|-------|----------|
| ECDSA round-trip | sign∘verify | msg, key | PASS |
| **RFC 6979 KAT** | deterministic đúng | vector chuẩn | chữ ký khớp byte-for-byte |
| RSA-PSS round-trip | — | msg, key | PASS |
| Modified message | integrity | sửa 1 byte msg | verify **FAIL** |
| Modified signature | — | sửa sig | FAIL |
| Modified pubkey | — | pub khác | FAIL |
| Wrong algo id | — | verify sai algo | FAIL/reject |
| Wrong hash | — | sign sha256, verify sha384 | FAIL |
| Batch verify | hiệu năng + đúng | N=1000 chữ ký | đếm PASS đúng |

**KAT nguồn:** RFC 6979 test vectors (ECDSA deterministic), NIST/RFC 8017 (RSA-PSS).

---

## 8. Benchmark Plan

- **Đo:** keygen / sign / verify latency + **throughput (ops/sec)** cho ECDSA-P256 & RSA-PSS-3072.
- **Message sizes:** 1KiB / 16KiB / 1MiB / 8MiB.
- **Phân tích (chính xác):**
  - **Hash cost dominance:** với msg lớn, thời gian gần như là hash (SHA-256) — sign/verify chỉ thao tác trên digest cố định.
  - **ECDSA:** sign nhanh, verify ~ chậm hơn sign chút; **chữ ký rất nhỏ** (~64–72B DER).
  - **RSA-PSS:** **verify rất nhanh** (e=65537), **sign chậm** (private exp); chữ ký **384B** (3072-bit).
  - Memory: EC key nhỏ hơn RSA nhiều.
- **Chuẩn bị bảng để Lab 6 dán cạnh ML-DSA** (sign/verify/size).

---

## 9. Security Analysis

**Bắt buộc bàn (đề):**
- **Deterministic nonce (RFC 6979) vs random:** random nonce sai RNG → reuse/bias → **lộ private key**. RFC 6979 sinh nonce từ `HMAC(key, msg)` → loại bỏ rủi ro RNG.
- **Nonce reuse catastrophe (ECDSA):** ký 2 msg khác nhau cùng `k` → giải hệ 2 phương trình → **recover private key** (vụ PS3 2010, ví Bitcoin bị rút).
- **Signature size:** ECDSA ≪ RSA-PSS (64B vs 384B).
- **Verify vs sign cost:** RSA verify ≪ sign; ECDSA sign ≲ verify.
- **PSS > PKCS#1 v1.5:** PSS có **security proof** (random oracle), salt randomized → probabilistic; v1.5 deterministic, dính nhiều biến thể tấn công (vd Bleichenbacher trên chữ ký với e nhỏ).
- **Constant-time verify + error handling:** so sánh/đường lỗi rò rỉ → side-channel.

**Bonus +15:** EC point arithmetic + modular inversion + RSA modexp + PSS padding từ primitive; timing attack surface; fault injection; side-channel modexp; basic timing variance measurement.

**Limitations:** dựa thư viện (constant-time do lib bảo đảm); chưa chống fault injection; key lưu test PEM.

---

## 10. Report Template (mục Lab 5)

Theo [11 mục](00_report_template.md#2-mẫu-11-mục-cho-mỗi-lab-lặp-lại-cho-lab-16). Riêng Lab 5:
- **Security:** sơ đồ "nonce reuse → recover key" (2 phương trình); RFC 6979 flow.
- **Performance:** bảng ECDSA vs RSA-PSS (keygen/sign/verify/size) + plot latency vs message size; **để chừa chỗ so với Lab 6**.
- **Negative:** bảng 5 ca + batch verify.
- **Hình:** sign/verify CLI, verify FAIL an toàn.

---

## 11. Common Mistakes

- ❌ ECDSA dùng **random nonce** không justify → đề yêu cầu RFC 6979.
- ❌ RSA-PSS salt length sai (phải **hashLen = 32B**) → verify fail chéo thư viện.
- ❌ Nhầm DER vs raw (r‖s) khi encode signature.
- ❌ Verify trả "true" khi exception nuốt mất (phải fail closed).
- ❌ Hash mismatch không kiểm → chữ ký "hợp lệ" với hash sai.
- ❌ So sánh sai bản chất: nói "RSA verify chậm" (sai — RSA **verify nhanh**, **sign chậm**).
- ❌ Quên batch verification (đề yêu cầu verify N).

---

## 12. Final Submission Checklist (Lab 5)

- [ ] ECDSA-P256 (RFC 6979) + RSA-PSS-3072(SHA-256, salt 32B) sign/verify đúng
- [ ] keygen PEM/DER; signature raw/DER/base64
- [ ] Validate algo id / hash mismatch / malformed key
- [ ] Negative 5 ca + batch verify (N)
- [ ] RFC 6979 KAT PASS (deterministic byte-for-byte)
- [ ] Benchmark keygen/sign/verify 4 size + ops/sec; bảng so EC vs RSA
- [ ] `ctest` pass Win+Linux; report Lab 5 đủ 11 mục + chừa so sánh Lab 6
- [ ] (Bonus) P-384 (+5) / formula-level (+15)

**Rubric Lab 5 (100 + 15):** ECDSA **20** · RSA-PSS **20** · Key handling & formats **15** · Negative & UX **10** · Performance **20** · Cross-platform & doc **5** · Report **10** · Bonus advanced **+15**.

> Tiếp theo → [Lab 6 — Post-Quantum](lab6_post_quantum.md).
