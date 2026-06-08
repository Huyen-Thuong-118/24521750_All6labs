# Lab 3 — RSA-OAEP & Hybrid Encryption

> **Tool:** `rsatool` · **Thư viện:** Crypto++ **hoặc** OpenSSL · **Rubric:** 100 (+15 bonus)
> Lab này dạy: vì sao RSA **không** mã hóa file lớn được, và **hybrid encryption** (envelope) — nền tảng của TLS/PGP.

---

## 1. Lab Overview

Xây `rsatool`: sinh khóa RSA, mã hóa/giải mã bằng **RSA-OAEP (SHA-256)**, và tự động chuyển sang **hybrid** (RSA bọc khóa AES-256-GCM) khi dữ liệu vượt giới hạn RSA. Đây chính là **envelope encryption** thực tế.

**Learning Outcomes (đề):** sinh & lưu key RSA an toàn; RSA-OAEP enc/dec; xử lý giới hạn plaintext + hybrid; phát hiện & xử lý lỗi crypto; benchmark RSA theo key size; hiểu vì sao hybrid là bắt buộc.

> 🔗 Tái dùng **AES-256-GCM** từ Lab 1. 🔗 Forward-secrecy limitation ở đây là cầu nối tới **ML-KEM (Lab 6)**.

---

## 2. Requirement Breakdown

### A. Chức năng
**Bắt buộc:**
- [ ] `keygen --bits 3072` → lưu **PEM + DER** + metadata JSON `{creation_time, modulus_bits, hash}`
- [ ] RSA-**3072** OAEP(SHA-256), MGF1(SHA-256); thêm **RSA-4096** để so hiệu năng
- [ ] OAEP **label** tùy chọn (`--label`)
- [ ] `encrypt`: nếu plaintext ≤ giới hạn → RSA-OAEP; nếu vượt → **tự động hybrid** + xuất envelope
- [ ] `decrypt`: tự nhận RSA thuần hay envelope
- [ ] Encoding ciphertext raw/hex/base64; keys PEM/DER

**Bonus:** manual OAEP (+5), hybrid security analysis (+10).

### B. Bảo mật
- [ ] AES-**256**-GCM, IV **96-bit**, authenticated decryption, reject tampered, constant-time tag verify (library-backed OK)
- [ ] RNG an toàn (AutoSeededRandomPool/RAND_bytes) cho AES key + IV
- [ ] Fail closed mọi lỗi parse/giải mã; **error message không leak** (không phân biệt "padding sai" vs "label sai" lộ liễu)

### C. Testing
- [ ] Negative: altered RSA ct→fail; altered GCM ct→tag fail; wrong priv key→fail; wrong OAEP label→fail; tampered envelope header→fail
- [ ] Unit test tự động; validate malformed ct / incorrect label / unsupported key size / incorrect encoding

### D. Benchmark
- [ ] RSA: keygen / encrypt / decrypt time; **3072 vs 4096**
- [ ] Hybrid: AES-GCM throughput; hybrid time cho **1KiB / 1MiB / 100MiB**

### E. Báo cáo
- [ ] OAEP an toàn (IND-CCA2); PKCS#1 v1.5 không an toàn; giới hạn `mLen ≤ k − 2·hLen − 2`; vì sao RSA không mã hóa file lớn

---

## 3. Folder Structure

```
lab3_rsa_hybrid/
├── CMakeLists.txt
├── include/
│   ├── rsa_oaep.hpp     # keygen, encrypt, decrypt (OAEP SHA-256)
│   ├── hybrid.hpp       # envelope: AES-256-GCM + RSA-wrap
│   ├── envelope.hpp     # serialize/parse JSON header
│   └── key_store.hpp    # PEM/DER load/save + metadata JSON
├── src/ (tương ứng .cpp + main.cpp)
├── tests/
│   ├── test_oaep.cpp
│   ├── test_hybrid.cpp
│   └── test_negative.cpp
├── kat/
│   └── rsa_oaep_vectors.json   # (nếu có vector cố định; OAEP ngẫu nhiên → test round-trip + decrypt KAT)
└── benchmark/bench_main.cpp
```

---

## 4. Architecture

```
                       ┌────────────── rsatool (CLI) ──────────────┐
 keygen ──► KeyStore ──┤ save PEM/DER + metadata.json              │
                       └───────────────────────────────────────────┘
 encrypt ─► size?
        ├─ ≤ limit ─► RsaOaep.encrypt(pub, pt) ─────────────► ct.bin (raw/hex/b64)
        └─ > limit ─► Hybrid.seal:
                         AES-256-GCM(dataKey, iv, pt) ─► payload + tag
                         RsaOaep.encrypt(pub, dataKey) ─► wrapped_key
                         Envelope.write{mode,rsa_modulus,hash,wrapped_key,iv,tag} + payload
 decrypt ─► detect(RSA | envelope) ─► RsaOaep.decrypt / Hybrid.open ─► pt
```
- **KeyStore:** I/O PEM/DER + metadata.
- **RsaOaep:** bọc API thư viện (OAEP SHA-256 + MGF1 + label).
- **Hybrid:** sinh dataKey 256-bit + IV 96-bit, AES-GCM, RSA-wrap key.
- **Envelope:** JSON header + payload binary.

---

## 5. Classes

| Class | Chức năng | Public methods | Private |
|-------|-----------|----------------|---------|
| `KeyStore` | PEM/DER + metadata | `genRsa(bits)`, `savePem/Der`, `loadPub/Priv`, `writeMetadata` | key objects |
| `RsaOaep` | RSA-OAEP SHA-256 | `encrypt(pub, msg, label?)`, `decrypt(priv, ct, label?)`, `maxMsgLen(bits)` | hash/MGF config |
| `Hybrid` | Envelope seal/open | `seal(pub, pt) → Envelope`, `open(priv, env) → pt` | AES-GCM, RNG |
| `Envelope` | JSON header (+payload) | `serialize()`, `parse(bytes)` | fields: mode, rsa_modulus, hash, wrapped_key, iv, tag |
| `BenchmarkRunner` (common) | đo RSA/hybrid | `measure(...)` | — |

---

## 6. Implementation Roadmap

| Step | Việc | "Done" khi |
|------|------|-----------|
| 1 | Setup + CMake link OpenSSL **hoặc** Crypto++ | `rsatool --help` |
| 2 | `keygen` PEM/DER + metadata.json | mở lại key OK; metadata đúng |
| 3 | `RsaOaep.encrypt/decrypt` (SHA-256, MGF1, label) | round-trip msg ngắn |
| 4 | Tính `maxMsgLen` + chặn msg quá dài (RSA thuần) | msg > limit → chuyển hybrid |
| 5 | `Hybrid.seal/open` (AES-256-GCM, IV 96-bit) | round-trip file lớn |
| 6 | `Envelope` JSON serialize/parse + base64 | header đúng format đề |
| 7 | `decrypt` auto-detect RSA vs envelope | cả 2 đường giải mã đúng |
| 8 | Negative tests + secure error | mọi ca fail closed, message không leak |
| 9 | Benchmark RSA 3072/4096 + hybrid 1KiB/1MiB/100MiB | bảng + plot |

---

## 7. Testing Plan

| Test | Mục tiêu | Input | Expected |
|------|----------|-------|----------|
| OAEP round-trip | enc∘dec=id | msg ngắn, pub/priv | khôi phục đúng |
| Label binding | label tham gia | encrypt label="A", decrypt label="B" | **fail** |
| Size limit → hybrid | tự chuyển mode | msg > `k−2·hLen−2` | xuất envelope, giải mã OK |
| GCM tag | integrity | sửa payload | **tag failure**, fail closed |
| Altered RSA ct | OAEP từ chối | lật bit wrapped_key | decrypt fail |
| Wrong priv key | — | priv khác | fail |
| Tampered header | parse an toàn | sửa `rsa_modulus`/`iv` | fail closed, không crash |
| Malformed/encoding | validate | base64 hỏng | reject rõ ràng |

**KAT/giới hạn:** RSA-3072, hLen=32 (SHA-256), `k = 384 byte` → **mLen ≤ 384 − 2·32 − 2 = 318 byte**. Ghi rõ con số này trong report.

---

## 8. Benchmark Plan

- **RSA:** keygen, encrypt, decrypt latency cho **3072 và 4096** (decrypt chậm hơn encrypt nhiều do exponent private lớn).
- **Hybrid:** AES-GCM throughput (MB/s) + tổng thời gian seal cho **1KiB / 1MiB / 100MiB**.
- **Phân tích bắt buộc:** dec vs enc cost; key size vs perf; **hybrid hiệu quả vượt trội** (RSA chỉ tốn 1 lần wrap key, phần còn lại là AES tốc độ cao); vì sao symmetric thống trị throughput; memory/CPU.
- **CSV:** thêm cột `op` (keygen/enc/dec/seal).

---

## 9. Security Analysis

**Threat model:** kẻ tấn công có public key + ciphertext/envelope, muốn đọc plaintext hoặc giả mạo.

**Bắt buộc bàn (đề):**
- **OAEP an toàn (IND-CCA2):** randomized padding + MGF1 → ciphertext không deterministic, chống chosen-ciphertext.
- **PKCS#1 v1.5 encryption KHÔNG an toàn:** dính **Bleichenbacher padding oracle** (1998) — rò rỉ qua phản hồi padding.
- **Giới hạn plaintext:** `mLen ≤ k − 2·hLen − 2` → RSA không mã hóa file lớn trực tiếp → **bắt buộc hybrid**.
- **Hybrid/envelope:** RSA bọc khóa, AES-GCM mã dữ liệu — đúng mô hình TLS/PGP.

**Implementation-level:** RNG an toàn cho dataKey/IV; **constant-time tag verify** (dựa thư viện); fail-closed; error **không leak** chi tiết (tránh tạo oracle); IV 96-bit GCM không reuse.

**Bonus +10 (hybrid analysis):** envelope vs direct RSA; TLS handshake dùng hybrid; **forward secrecy** — RSA key-transport **không** có FS (lộ priv → giải mọi phiên cũ) → nâng cấp **ECDHE** hoặc **ML-KEM (Lab 6)**; so sánh AES-CTR+HMAC vs AES-GCM (đo timing variance auth fail).

**Limitations:** key RSA lưu PEM không mã hóa passphrase (test key); chưa có FS; không chống replay ở mức envelope.

---

## 10. Report Template (mục Lab 3)

Theo [11 mục](00_report_template.md#2-mẫu-11-mục-cho-mỗi-lab-lặp-lại-cho-lab-16). Riêng Lab 3:
- **System Design:** sơ đồ envelope; cấu trúc JSON header.
- **Implementation:** công thức `mLen ≤ k−2hLen−2` + con số RSA-3072 (318B).
- **Negative Testing:** bảng 5 ca đề yêu cầu + ảnh "tag failure".
- **Performance:** bảng RSA 3072 vs 4096 (keygen/enc/dec) + hybrid 3 size; biểu đồ.
- **Security:** OAEP IND-CCA2, Bleichenbacher, forward secrecy → ML-KEM.
- **Hình:** metadata.json, envelope.json, decrypt fail an toàn.

---

## 11. Common Mistakes

- ❌ Dùng **PKCS#1 v1.5** thay OAEP (mất điểm + sai đề).
- ❌ Quên label phải khớp khi decrypt.
- ❌ GCM IV không phải 96-bit / reuse IV.
- ❌ Không tự chuyển hybrid khi msg vượt giới hạn → encrypt fail với file lớn.
- ❌ Error message leak ("OAEP padding error at byte 5") tạo oracle.
- ❌ Lưu wrapped_key/iv/tag sai encoding (phải base64 trong JSON header).
- ❌ DER/PEM lẫn lộn khi load.
- ❌ Trộn 2 thư viện (chọn 1: OpenSSL **hoặc** Crypto++, nhất quán).

---

## 12. Final Submission Checklist (Lab 3)

- [ ] `keygen` PEM+DER+metadata; RSA-3072 OAEP(SHA-256) + RSA-4096
- [ ] OAEP label tùy chọn; round-trip OK
- [ ] Auto-hybrid khi vượt giới hạn; envelope JSON đúng format
- [ ] AES-256-GCM, IV 96-bit, tag verify, fail closed
- [ ] Negative: 5 ca đề + malformed/encoding/keysize
- [ ] Error message không leak
- [ ] Benchmark RSA 3072/4096 + hybrid 1KiB/1MiB/100MiB + plot
- [ ] `ctest` pass Win+Linux; report Lab 3 đủ 11 mục
- [ ] (Bonus) manual OAEP (+5), hybrid analysis (+10)

**Rubric Lab 3 (100 + 15):** RSA-OAEP **20** · Hybrid envelope **20** · Key mgmt & formats **15** · Negative & secure error **15** · Performance **15** · Cross-platform & doc **5** · Report **10** · Bonus manual OAEP **+5**, hybrid analysis **+10**.

> Tiếp theo → [Lab 4 — Hash & PKI](lab4_hash_pki.md).
