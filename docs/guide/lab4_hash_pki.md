# Lab 4 — Hashing, PKI & Practical Attacks

> **Tool:** `hashtool` · **Thư viện:** OpenSSL hoặc Crypto++ · **Rubric:** 100 (+15 bonus)
>
> ⚠️ **ETHICS (đề bắt buộc):** Lab này **chỉ phục vụ giáo dục phòng thủ**. Mọi demo tấn công (MD5 collision, length-extension, chosen-prefix) **chỉ trên file của chính bạn, trong môi trường offline/cô lập**. **KHÔNG** nhắm vào hệ thống/dịch vụ thật. Đây là điều kiện tiên quyết — vi phạm bị phạt học thuật.

---

## 1. Lab Overview

Ba phần: (1) **Hashing suite** (SHA-2/SHA-3/SHAKE) với KAT + streaming; (2) **PKI** — parse & verify X.509, triển khai HTTPS thật; (3) **Practical attacks minh họa** — MD5 collision (bằng `hashclash`) và length-extension (bằng `hashpump` hoặc tự code) để **hiểu vì sao hash cũ bị cấm** và **cách phòng** (HMAC, SHA-2/3, policy).

**Learning Outcomes (đề):** implement & benchmark hash hiện đại; parse/validate X.509; demo MD5 collision an toàn; demo length-extension trên MAC ngây thơ; giải thích vì sao hash legacy không an toàn; đề xuất mitigation.

---

## 2. Requirement Breakdown

### A. Chức năng — Hashing suite
**Bắt buộc:**
- [ ] SHA-2: **224/256/384/512** · SHA-3: **224/256/384/512** · SHAKE: **128/256** (variable, `--outlen`)
- [ ] `hashtool --algo sha256 --in file.bin`; `--stream` (file nhiều GB); `--algo shake256 --outlen 64`
- [ ] Output: **hex** ra console, **raw** ra file; validate algo id; **fail closed** param không hỗ trợ

### B. PKI
- [ ] Parse X.509: **Subject, Issuer, SPKI (algo+params), signature algorithm, validity period, key usage, SANs**
- [ ] Verify chữ ký cert bằng issuer public key (nếu có); nếu không có issuer key → verify TBS integrity + algorithm consistency + **fail closed**
- [ ] **TLS deployment:** HTTPS trên **Apache/Nginx**, TLS **1.2/1.3**, trusted root (vd ZeroSSL), cert **ECDSA** (ưu tiên) → nộp screenshot/log + config snippet + giải thích trust chain

### C. Practical attacks (offline)
- [ ] **MD5 collision** (hashclash): chọn **2 PNG benign** *hoặc* **2 chương trình C++** cùng MD5 → nộp 2 file + digest trùng + giải thích 1 trang
- [ ] **Length-extension** trên `MAC=H(k‖m)` (hashpump hoặc tự code +5): cho `H(k‖m)` + `len(k)` → tính `H(k‖m‖pad‖m')` mà **không biết k** → nộp msg gốc, forged msg, forged MAC, sơ đồ padding, mitigation

### D. Benchmark
- [ ] SHA-256, SHA-512, SHA3-256, SHA3-512; sizes **1MiB/100MiB/1GiB**; streaming vs mmap; SHA-2 vs SHA-3; cả 2 OS

### E. Báo cáo
- [ ] Merkle–Damgård vs sponge; collision vs preimage; vì sao SHA-3 khác SHA-2; XOF use case; chain of trust; vì sao MD5/SHA-1 bị cấm; CT & CA/B

**Bonus:** custom length-extension SHA-256 (+5); chosen-prefix MD5 cert demo (+10, **sandbox**).

---

## 3. Folder Structure

```
lab4_hash_pki/
├── CMakeLists.txt
├── include/
│   ├── hasher.hpp        # SHA-2/3/SHAKE qua thư viện + streaming
│   └── x509_parser.hpp   # parse + verify cert
├── src/ (+ main.cpp)
├── tests/
│   ├── test_hash_kat.cpp
│   ├── test_x509.cpp
│   └── test_negative.cpp
├── kat/
│   ├── sha2.json  sha3.json  shake.json   # NIST CAVP vectors
├── certificates/
│   ├── sample.pem            # cert để parse
│   ├── self_signed_test.pem  # cert tự ký test
│   └── md5_collision/        # 2 file demo + digest (Option A/B)
├── attacks/
│   ├── length_ext/           # script + artifact length-extension
│   └── md5_collision/        # hashclash artifacts
└── benchmark/bench_main.cpp
```

---

## 4. Architecture

```
                 ┌──────── hashtool (CLI) ────────┐
 --algo --in ──► │ AlgoRegistry → Hasher          │── hex(console)/raw(file)
 --stream        │   (SHA-2 / SHA-3 / SHAKE)      │
 --outlen        └────────────────────────────────┘
                 ┌──────── certtool / parse ──────┐
 --cert ───────► │ X509Parser → fields + verify   │── report fields, PASS/FAIL
                 └────────────────────────────────┘
 (offline demos: hashclash, hashpump — chạy ngoài, artifact đưa vào attacks/)
```

---

## 5. Classes

| Class | Chức năng | Public methods | Private |
|-------|-----------|----------------|---------|
| `Hasher` | Hash 1-shot + streaming | `digest(algo, data)`, `stream(algo, istream, outlen?)` | ctx thư viện |
| `AlgoRegistry` | map tên→algo, validate | `resolve(name)`, `isXof(name)` | bảng |
| `X509Parser` | parse + verify cert | `parse(pem) → CertInfo`, `verify(cert, issuerPub?)` | OpenSSL X509* |
| `KatRunner`/`BenchmarkRunner` (common) | KAT + bench | — | — |

---

## 6. Implementation Roadmap

| Step | Việc | "Done" khi |
|------|------|-----------|
| 1 | Setup + link OpenSSL; `Hasher` SHA-256 | `hashtool --algo sha256` đúng KAT |
| 2 | Thêm SHA-2 family + SHA-3 + SHAKE (`--outlen`) | tất cả PASS KAT |
| 3 | Streaming `--stream` (đọc block, không nạp cả file) | hash file 1GB không OOM |
| 4 | `X509Parser` extract 7 trường | in đủ Subject/Issuer/SPKI/... |
| 5 | Verify chữ ký cert (issuer key / TBS fallback) | PASS/FAIL hợp lý, fail closed |
| 6 | TLS deploy Nginx/Apache + ECDSA cert | trình duyệt xanh, lấy screenshot |
| 7 | MD5 collision demo (hashclash) | 2 file khác, cùng MD5 |
| 8 | Length-extension demo (hashpump/tự code) | forged MAC verify đúng trên MAC ngây thơ |
| 9 | Benchmark 4 hash + plot | SHA-2 vs SHA-3 |

---

## 7. Testing Plan

| Test | Mục tiêu | Input | Expected |
|------|----------|-------|----------|
| Hash KAT | đúng chuẩn | NIST CAVP vectors | digest khớp |
| Empty/known | sanity | `""` SHA-256 | `e3b0c442...b855` |
| SHAKE outlen | XOF | shake256 outlen 64 | đúng 64 byte, khớp KAT |
| Stream == 1-shot | streaming đúng | file lớn | digest giống bản 1-shot |
| X.509 parse | trích đúng | sample.pem | Subject/Issuer/SAN... đúng |
| Cert verify | chữ ký | cert + issuer key | PASS; sửa TBS → FAIL |
| Malformed | fail closed | PEM hỏng, algo lạ | reject rõ ràng |

**KAT nguồn:** NIST **CAVP** (SHA-2, SHA-3, SHAKE).

---

## 8. Benchmark Plan

- **Hash:** SHA-256, SHA-512, SHA3-256, SHA3-512; sizes 1MiB/100MiB/1GiB.
- **So:** streaming vs memory-mapped I/O; SHA-2 vs SHA-3 (SHA-3 thường chậm hơn trên CPU không có hardware Keccak); CPU utilization; cache effects; Windows vs Linux.
- **Plot throughput (MB/s).**

---

## 9. Security Analysis

**Bắt buộc bàn (đề):**
- **Merkle–Damgård (SHA-2) vs Sponge (SHA-3):** MD nén tuần tự với chaining state lộ ra ở digest → **length-extension**; sponge (absorb/squeeze) **miễn nhiễm** length-extension.
- **Collision vs preimage resistance:** collision = tìm `x≠y, H(x)=H(y)` (sinh nhật, ~2^(n/2)); preimage = từ `h` tìm `x` (~2^n). MD5 **vỡ collision** (giây), chưa vỡ preimage — nhưng đủ để cấm.
- **Length-extension:** vì sao MD cho phép (state = digest) → từ `H(k‖m)` nối tiếp được; **HMAC** chống nhờ cấu trúc `H(k⊕opad ‖ H(k⊕ipad ‖ m))`; prefix-free constructions.
- **MD5/SHA-1 bị cấm:** chosen-prefix collision → giả mạo chứng chỉ (Flame malware 2012, sự cố Rogue CA 2008).
- **PKI:** X.509 = **TBS** (to-be-signed) + signature; **chain of trust** (leaf→intermediate→root); **Certificate Transparency** + **CA/Browser Forum Baseline Requirements**.

**Mitigations:** HMAC thay `H(k‖m)`; dùng SHA-256/SHA-3; ban MD5/SHA-1; algorithm agility; enforce CA/B BR.

**Limitations:** demo dùng tool có sẵn (hashclash/hashpump) — không tự sinh collision từ đầu (trừ bonus); cert verify offline không kiểm CRL/OCSP.

---

## 10. Report Template (mục Lab 4)

Theo [11 mục](00_report_template.md#2-mẫu-11-mục-cho-mỗi-lab-lặp-lại-cho-lab-16). Riêng Lab 4:
- **KAT:** bảng coverage SHA-2/3/SHAKE.
- **PKI:** ảnh parse cert (7 trường) + ảnh HTTPS xanh + config snippet + sơ đồ chain of trust.
- **MD5 collision:** 2 file + digest trùng + giải thích 1 trang (collision vs preimage + sự cố lịch sử).
- **Length-extension:** sơ đồ padding (`0x80 ‖ 0x00... ‖ len64`), msg gốc/forged/MAC, vì sao HMAC chống.
- **Performance:** SHA-2 vs SHA-3 plot.

---

## 11. Common Mistakes

- ❌ SHAKE quên `--outlen` (XOF cần độ dài ra rõ ràng).
- ❌ Streaming nạp cả file vào RAM → OOM với 1GB.
- ❌ In digest sai endianness/định dạng (phải hex lowercase).
- ❌ X.509: chỉ parse mà **không verify** chữ ký (mất nửa điểm PKI).
- ❌ TLS: dùng RSA cert thay vì ECDSA (đề ưu tiên ECDSA); quên screenshot chain.
- ❌ Length-extension: nhầm độ dài padding/`len` bit vs byte.
- ❌ Demo tấn công ra ngoài sandbox / nhắm hệ thống thật → **vi phạm đạo đức**.
- ❌ Dùng MD5/SHA-1 cho mục đích thật (chỉ dùng để **minh họa điểm yếu**).

---

## 12. Final Submission Checklist (Lab 4)

- [ ] SHA-2/3/SHAKE đầy đủ; `--outlen`; `--stream`; KAT all PASS
- [ ] X.509 parse 7 trường + verify chữ ký (+ TBS fallback)
- [ ] HTTPS deploy (Nginx/Apache, TLS1.2/1.3, ECDSA) + screenshot + config + trust chain
- [ ] MD5 collision: 2 file + digest trùng + giải thích 1 trang
- [ ] Length-extension: artifact đầy đủ + sơ đồ padding + mitigation
- [ ] Benchmark 4 hash + streaming vs mmap + plot (cả 2 OS)
- [ ] Ethics statement; mọi demo **offline, artifact của mình**
- [ ] `ctest` pass; report Lab 4 đủ 11 mục
- [ ] (Bonus) custom length-extension SHA-256 (+5); chosen-prefix cert (+10, sandbox)

**Rubric Lab 4 (100 + 15):** Hash suite & KATs **25** · Streaming & large-file perf **10** · X.509 parse & verify **25** · MD5 collision demo & explanation **20** · Length-extension demo & mitigation **20** · Bonus custom LE **+5**, chosen-prefix **+10**.

> Tiếp theo → [Lab 5 — Signatures](lab5_signatures.md).
