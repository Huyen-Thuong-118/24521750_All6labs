# Lab 2 — AES-128 thuần C++ (CTR Mode, FIPS-197)

> **Tool:** `aestool` (bản tự code) · **Thư viện:** KHÔNG dùng thư viện crypto nào (chỉ STL) · **Rubric:** 100 (+20 bonus)
> Đây là lab "hiểu sâu": bạn **tự tay viết AES** từ FIPS-197, không gọi Crypto++/OpenSSL.

---

## 1. Lab Overview

Tự implement **AES-128** (block cipher) theo **FIPS-197** và **CTR mode** theo **NIST SP 800-38A**, hoàn toàn bằng C++ thuần (STL được phép). Sau đó **cross-validate**: ciphertext của bạn phải **trùng khít** với Crypto++/OpenSSL AES-128-CTR cùng key+IV.

**Ràng buộc:** Cấm mọi thư viện crypto (OpenSSL, Crypto++, libsodium...). AES-NI chỉ được dùng trong phần bonus.

**Learning Outcomes (đề):** implement AES round transforms; CTR đúng chuẩn; validate bằng NIST KAT; hiểu nonce misuse & stream-cipher risk; benchmark AES phần mềm; phân tích side-channel của table-based S-box; hiểu vì sao CTR cần authentication.

> 🔗 Dùng key+IV giống Lab 1 để cross-check ciphertext.

---

## 2. Requirement Breakdown

### A. Chức năng
**Bắt buộc:** AES-128 (block 128-bit, key 128-bit, **10 rounds**) với đủ `AddRoundKey, SubBytes, ShiftRows, MixColumns, KeyExpansion`; **CTR mode**; key **đúng 16 byte**, IV **đúng 16 byte**; input/output **raw binary** (hex để debug); hỗ trợ plaintext **độ dài bất kỳ** + **partial final block**, **không padding**.
**Bonus:** AES-192/256 (+10, full key schedule + KAT), AES-XTS (+5), AES-NI/bit-sliced (+5).

### B. Bảo mật
- [ ] IV (nonce) **unique per key** — bàn rõ reuse là thảm họa
- [ ] Validate: key length, IV length, counter overflow, file I/O error → **fail closed**
- [ ] (Bonus/Discussion) constant-time consideration cho S-box

### C. Testing
- [ ] KAT: **FIPS-197** AES vectors + **NIST SP 800-38A CTR** vectors
- [ ] **Cross-validation** với Crypto++/OpenSSL AES-128-CTR (cùng key+IV → ciphertext y hệt)
- [ ] Negative: wrong key, wrong IV, tampered ct (+ giải thích vì sao CTR không phát hiện được)

### D. Benchmark
- [ ] AES-128 enc & dec throughput; sizes **1MiB, 100MiB, 1GiB** (nếu khả thi)
- [ ] Phân tích: table-based vs computed S-box, memory footprint, cache, branch prediction, so với Crypto++

### E. Báo cáo
- [ ] AES core risks (timing/T-table/constant-time) + CTR risks + AES-NI

---

## 3. Folder Structure

```
lab2_aes_manual/
├── CMakeLists.txt
├── include/
│   ├── aes_core.hpp     # AES-128 block: encryptBlock/decryptBlock + KeyExpansion
│   ├── aes_tables.hpp   # S-box, inv S-box, Rcon (precomputed)
│   ├── gf256.hpp        # số học GF(2^8): xtime, mul
│   └── ctr_mode.hpp     # CTR streaming
├── src/
│   ├── main.cpp
│   ├── aes_core.cpp
│   ├── ctr_mode.cpp
│   └── gf256.cpp
├── tests/
│   ├── test_fips197.cpp     # vector chuẩn FIPS-197
│   ├── test_ctr.cpp         # SP 800-38A CTR
│   ├── test_crossval.cpp    # so với Crypto++ (test build, không link vào tool chính)
│   └── test_negative.cpp
├── kat/
│   ├── fips197.json
│   └── sp800-38a-ctr.json
└── benchmark/
    └── bench_main.cpp
```

> 💡 `test_crossval.cpp` được phép link Crypto++ vì nó **chỉ ở test**, không phải trong `aestool`. Tool chính tuyệt đối không link crypto lib.

---

## 4. Architecture

```
  CLI ──► ArgumentParser ──► (KeyManager 16B) ──► AesCore (KeyExpansion → roundKeys)
                                                       │
   IV 16B ──► CtrMode.process(stream) ──────────────► encryptBlock(counter) ─► keystream
                                                       │
                       plaintext ───────────── XOR ───┴───► ciphertext ──► File (--out)
```
- **AesCore:** giữ 11 round keys (44 words), cung cấp `encryptBlock(16B)→16B`. CTR **chỉ cần encrypt** (cả enc lẫn dec đều XOR keystream).
- **CtrMode:** chia stream thành block 16B, sinh keystream `S_i = AES(K, IV||counter_i)`, XOR với data; xử lý block cuối thiếu.
- **GF256:** `xtime`, `mul` cho MixColumns.

---

## 5. Classes

| Class | Chức năng | Public methods | Private members |
|-------|-----------|----------------|-----------------|
| `AesCore` | AES-128 block cipher | `AesCore(key16)`, `encryptBlock(in16)→out16`, `decryptBlock(...)` | `roundKeys_[44]`, `keyExpansion()` |
| `CtrMode` | Stream qua CTR | `process(in, out, iv16)` | `AesCore& aes_`, `increment(counter)` |
| `Gf256` | Số học GF(2^8) | `static xtime(b)`, `static mul(a,b)` | — |
| `KeyManager`/`NonceManager` | validate 16B | như Lab 1 (rút gọn) | — |
| `KatRunner`/`BenchmarkRunner` (common) | KAT + bench | (dùng chung) | — |

---

## 6. Lý thuyết AES — giải thích từng bước (PHẦN CỐT LÕI)

> AES làm việc trên **State**: ma trận **4×4 byte** (16 byte = 128 bit). Quy ước **column-major**: byte input thứ 0,1,2,3 là **cột 0** (từ trên xuống), 4,5,6,7 là cột 1, v.v.
>
> ```
>        cột0 cột1 cột2 cột3
> hàng0  s00  s01  s02  s03      input[0]→s00, input[1]→s10,
> hàng1  s10  s11  s12  s13      input[2]→s20, input[3]→s30,
> hàng2  s20  s21  s22  s23      input[4]→s01, ...
> hàng3  s30  s31  s32  s33
> ```

### Cấu trúc 10 vòng AES-128
```
AddRoundKey(state, roundKey[0])                 ← vòng "0"
for r = 1..9:
    SubBytes → ShiftRows → MixColumns → AddRoundKey(roundKey[r])
SubBytes → ShiftRows → AddRoundKey(roundKey[10])   ← vòng cuối, KHÔNG MixColumns
```
Giải mã = làm ngược: InvShiftRows, InvSubBytes, AddRoundKey, InvMixColumns (thứ tự đảo). **Với CTR bạn chỉ cần `encryptBlock`** → có thể bỏ qua decrypt nếu chỉ làm CTR (nhưng nên có để test FIPS-197 block).

### 6.1. AddRoundKey
**XOR** từng byte của state với round key tương ứng. Đây là bước duy nhất dùng key.
```cpp
for (int i = 0; i < 16; ++i) state[i] ^= roundKey[i];
```
Vì XOR là tự nghịch đảo, AddRoundKey khi giải mã y hệt khi mã hóa.

### 6.2. SubBytes (thay thế phi tuyến — S-box)
Mỗi byte `b` → `S-box[b]`. S-box = (1) nghịch đảo nhân trong GF(2^8) (0 → 0), rồi (2) một **affine transform**. Trong lab, **dùng bảng S-box 256 phần tử precomputed** (bonus: tính constant-time).
```cpp
for (auto& b : state) b = SBOX[b];   // InvSubBytes dùng INV_SBOX
```
**Ví dụ:** `0x53 → S-box → 0xED`; `0x00 → 0x63`.
> ⚠️ **Side-channel:** truy cập `SBOX[b]` phụ thuộc dữ liệu → rò rỉ qua cache timing (T-table attack). Bàn trong báo cáo.

### 6.3. ShiftRows (hoán vị byte theo hàng)
Hàng `r` dịch **trái vòng** `r` vị trí: hàng 0 giữ nguyên, hàng 1 dịch 1, hàng 2 dịch 2, hàng 3 dịch 3.
```
trước:            sau ShiftRows:
s00 s01 s02 s03   s00 s01 s02 s03
s10 s11 s12 s13   s11 s12 s13 s10
s20 s21 s22 s23   s22 s23 s20 s21
s30 s31 s32 s33   s33 s30 s31 s32
```
InvShiftRows dịch **phải**. Lưu ý chỉ số: nếu lưu state mảng 16 byte column-major, hàng `r` gồm các index `r, r+4, r+8, r+12`.

### 6.4. MixColumns (trộn trong cột — khuếch tán)
Mỗi **cột** (4 byte) nhân với ma trận cố định trong GF(2^8):
```
| s0' |   | 02 03 01 01 | | s0 |
| s1' | = | 01 02 03 01 | | s1 |     (phép nhân & cộng trong GF(2^8))
| s2' |   | 01 01 02 03 | | s2 |
| s3' |   | 03 01 01 02 | | s3 |
```
Cộng = XOR. Nhân với 01 = chính nó; nhân 02 = `xtime`; nhân 03 = `xtime(x) ^ x`.
```cpp
uint8_t xtime(uint8_t x){ return (x<<1) ^ ((x>>7)*0x1B); } // mod x^8+x^4+x^3+x+1 (0x11B)
// cột {s0,s1,s2,s3}:
t0 = xtime(s0)^(xtime(s1)^s1)^s2^s3;   // 02·s0 ^ 03·s1 ^ 01·s2 ^ 01·s3
t1 = s0^xtime(s1)^(xtime(s2)^s2)^s3;
t2 = s0^s1^xtime(s2)^(xtime(s3)^s3);
t3 = (xtime(s0)^s0)^s1^s2^xtime(s3);
```
InvMixColumns dùng ma trận [0e 0b 0d 09]. Vòng cuối **bỏ MixColumns** (rất hay quên!).
> **GF(2^8):** byte = đa thức bậc ≤7. Nhân rồi **mod đa thức bất khả quy** `m(x)=x^8+x^4+x^3+x+1` (0x11B). `xtime` = nhân `x` (dịch trái 1, nếu tràn bit 7 thì XOR 0x1B).

### 6.5. KeyExpansion (sinh 11 round key)
AES-128: key 16 byte = 4 word (mỗi word 4 byte). Sinh tổng **44 word** `w[0..43]`, mỗi round key = 4 word.
```
w[0..3] = key
for i = 4..43:
    temp = w[i-1]
    if i % 4 == 0:
        temp = SubWord(RotWord(temp)) XOR Rcon[i/4]
    w[i] = w[i-4] XOR temp
```
- **RotWord:** `[a0,a1,a2,a3] → [a1,a2,a3,a0]` (xoay byte).
- **SubWord:** áp S-box cho 4 byte.
- **Rcon[j]:** `[rc, 00, 00, 00]` với `rc = 1,2,4,8,...` (nhân 2 trong GF(2^8)): `01,02,04,08,10,20,40,80,1B,36`.

> **Bonus AES-192/256:** key 6/8 word, 12/14 rounds; với AES-256 có thêm bước SubWord ở vị trí `i%8==4`. Đọc kỹ FIPS-197 §5.2.

### 6.6. CTR Mode (NIST SP 800-38A)
Biến AES thành **stream cipher**:
```
S_i = AES_encrypt(K, IV || Counter_i)
C_i = P_i XOR S_i           ;  P_i = C_i XOR S_i   (giải mã y hệt mã hóa)
```
- **IV 128-bit** = (Nonce ‖ Counter). Counter **tăng dần** mỗi block.
- **Document rõ:** counter **endianness** (thường big-endian, tăng ở byte cuối) và **overflow behavior** (wrap/abort).
- **Độ dài bất kỳ:** block cuối thiếu thì chỉ XOR số byte tương ứng (cắt keystream). **Không padding.**
```cpp
void increment(uint8_t ctr[16]){ for(int i=15;i>=0;--i) if(++ctr[i]) break; } // big-endian +1
```
> 🔴 **Quy tắc sống còn:** IV (nonce) phải **duy nhất per key**. Reuse → cùng keystream → two-time pad → lộ XOR hai plaintext. Đây là phần điểm Security.

---

## 7. Implementation Roadmap

| Step | Việc | "Done" khi |
|------|------|-----------|
| 1 | Setup `lab2/` + CMake (no crypto lib) + `gf256` (xtime/mul) | unit test xtime đúng |
| 2 | Bảng S-box/inv/Rcon + `SubBytes/ShiftRows/MixColumns/AddRoundKey` | từng transform test riêng |
| 3 | `KeyExpansion` → 44 word | round key khớp FIPS-197 Appendix A |
| 4 | `encryptBlock` (10 vòng) + `decryptBlock` | **FIPS-197 vector**: pt `3243f6a8...` → ct `3925841d02dc09fbdc118597196a0b32` |
| 5 | `CtrMode.process` + increment + partial block | round-trip CTR độ dài lẻ |
| 6 | CLI + validate 16B key/IV + fail closed | sai length → reject |
| 7 | KAT runner (FIPS-197 + SP800-38A CTR) | all PASS |
| 8 | Cross-validation với Crypto++/OpenSSL (chỉ ở test) | ciphertext **identical** |
| 9 | Negative tests | wrong key/iv → sai; tampered → hỏng (giải thích) |
| 10 | Benchmark (table vs computed S-box) → CSV + chart | có số liệu so sánh |

---

## 8. Testing Plan

| Test | Mục tiêu | Input | Expected |
|------|----------|-------|----------|
| FIPS-197 block | AES core đúng | key `2b7e1516...`, pt `3243f6a8...` | ct `3925841d02dc09fbdc118597196a0b32` |
| KeyExpansion | round key đúng | key FIPS-197 | khớp Appendix A |
| CTR round-trip | enc∘dec=id | key+iv, pt độ dài lẻ (vd 100B) | khôi phục đúng |
| **Cross-val** | trùng thư viện chuẩn | cùng key+iv | ct == Crypto++/OpenSSL AES-128-CTR |
| Wrong key | confidentiality | dec sai key | plaintext sai |
| Wrong IV | — | dec sai iv | plaintext sai (lệch từ block đầu) |
| Tampered ct | CTR không phát hiện | lật 1 bit ct | đúng 1 bit pt bị lật, **không báo lỗi** → giải thích CTR thiếu integrity |
| Invalid key/iv length | fail closed | key 8B | reject |

**KAT nguồn:** FIPS-197 (Appendix B/C) + NIST SP 800-38A §F.5 (CTR-AES128).

---

## 9. Benchmark Plan

- **Đo:** AES-128 encrypt & decrypt throughput; sizes **1MiB, 100MiB, 1GiB** (nếu RAM/đĩa cho phép).
- **Hai biến thể để so:** S-box **table-based** vs **computed/bit-sliced** (bonus); và so với **Crypto++** (chỉ để tham chiếu, build riêng).
- **Phân tích bắt buộc:** memory footprint (bảng T-table tốn cache), cache behavior, branch prediction (CTR ít branch), vì sao thư viện nhanh hơn (AES-NI).
- **CSV + plot throughput (MB/s).**

---

## 10. Security Analysis

**AES core risks:**
- **Timing leakage** từ S-box table-based: thời gian truy cập phụ thuộc giá trị byte → rò rỉ.
- **Cache attack (T-table):** kẻ tấn công cùng máy suy ra key qua cache access pattern.
- **Constant-time coding:** S-box tính bằng logic (bit-sliced) thay vì tra bảng → không rò.

**CTR mode risks:**
- **Keystream reuse / two-time pad:** reuse IV cùng key → `C1 ⊕ C2 = P1 ⊕ P2`.
- **IV reuse = catastrophic.**
- **CTR chỉ cho confidentiality**, **không** integrity → **không phát hiện sửa đổi** (lật bit ct = lật bit pt). Vì vậy **phải kèm MAC** (Encrypt-then-MAC) → CTR **không phải AEAD**.

**Hardware acceleration:** AES-NI tính AES trong phần cứng, **không tra bảng** → giảm side-channel + nhanh hơn nhiều; nêu trade-off security/performance.

**Limitations:** bản phần mềm table-based không constant-time; chưa có authentication; AES-128 (bonus mới có 192/256).

---

## 11. Report Template (mục Lab 2 trong báo cáo)

Theo [11 mục](00_report_template.md#2-mẫu-11-mục-cho-mỗi-lab-lặp-lại-cho-lab-16). Riêng Lab 2:
- **System Design:** sơ đồ State 4×4, luồng 10 vòng, layout mảng 16 byte (column-major).
- **Implementation:** giải thích 5 transform + KeyExpansion (dùng lại nội dung mục 6); ví dụ số FIPS-197.
- **KAT Validation:** ảnh FIPS-197 vector PASS + SP800-38A CTR + **cross-validation** với Crypto++/OpenSSL (cùng key+IV → ct identical).
- **Negative Testing:** bảng wrong key/iv/tampered + giải thích vì sao CTR không phát hiện sửa đổi.
- **Performance:** bảng throughput table-based vs computed S-box + so Crypto++; plot.
- **Security Analysis:** T-table side-channel, two-time pad, vì sao CTR cần MAC, vai trò AES-NI.
- **Hình cần chụp:** FIPS-197 test PASS; cross-validation identical; benchmark.

---

## 12. Common Mistakes

- ❌ **Quên bỏ MixColumns ở vòng cuối** → ciphertext sai. Lỗi #1.
- ❌ Nhầm thứ tự nạp State (row-major thay vì **column-major**).
- ❌ `xtime` quên modulo 0x1B khi tràn bit 7.
- ❌ Sai chỉ số ShiftRows do nhầm layout mảng 16 byte.
- ❌ Rcon sai (phải nhân 2 trong GF(2^8): ...,0x80,**0x1B**,0x36 — không phải 0x100).
- ❌ Counter increment sai endianness → ciphertext không khớp Crypto++ (cross-val fail).
- ❌ Block cuối: XOR cả 16 byte keystream thay vì chỉ phần dư.
- ❌ Lỡ link/`#include` thư viện crypto trong `aestool` → **phạm ràng buộc**.
- ❌ So ciphertext bằng chuỗi hex hoa/thường lẫn lộn.

---

## 13. Final Submission Checklist (Lab 2)

- [ ] AES-128 block khớp **FIPS-197** (pt `3243f6a8...` → `3925841d...`)
- [ ] Đủ 5 transform + KeyExpansion đúng Appendix A
- [ ] CTR đúng: arbitrary length + partial block + no padding + document endianness/overflow
- [ ] **Cross-validation** ciphertext identical với Crypto++/OpenSSL
- [ ] KAT FIPS-197 + SP800-38A CTR all PASS
- [ ] Validate key/iv 16B, fail closed
- [ ] Negative tests + giải thích CTR không phát hiện tampering
- [ ] Benchmark 1MiB/100MiB/(1GiB) + table vs computed S-box + so Crypto++
- [ ] Tool chính **không** link crypto lib (grep sạch)
- [ ] `ctest` pass Win+Linux; chương report đủ 11 mục
- [ ] (Bonus) AES-192/256 (+10), XTS (+5), AES-NI (+5)

**Rubric Lab 2 (100 + 20):** FIPS-197 AES-128 core **30** · CTR correctness **15** · KATs & cross-val **15** · Engineering & doc **10** · Performance **10** · Security discussion **20** · Bonus AES-192/256 **+10**, XTS **+5**, AES-NI/bit-sliced **+5**.

> Tiếp theo → [Lab 3 — RSA & Hybrid](lab3_rsa_hybrid.md).
