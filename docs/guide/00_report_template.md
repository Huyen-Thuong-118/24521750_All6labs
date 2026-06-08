# 00 — Mẫu báo cáo chung (1 file cho cả 6 lab)

> ⚠️ Đề yêu cầu **MỘT báo cáo duy nhất** (PDF/DOCX) bao trùm cả 6 lab, độ dài kỳ vọng **~60 trang**. Đừng viết 6 report rời. File này cho bạn: (a) bố cục tổng, (b) mẫu 11 mục lặp cho **mỗi lab**, (c) performance protocol copy vào mọi lab, (d) checklist ảnh/bảng/biểu đồ, (e) 2 appendix bắt buộc.

---

## 1. Bố cục báo cáo tổng

```
TRANG BÌA: Họ tên, MSV 24521750, môn, ngày
MỤC LỤC (tự động)

PHẦN 0 — GIỚI THIỆU CHUNG
  0.1 Môi trường thử nghiệm (bảng Hardware/OS — DÙNG CHUNG cho mọi benchmark)
  0.2 Cấu trúc source code & cách build (tóm tắt, trỏ README)
  0.3 Phương pháp benchmark chuẩn (Performance Protocol — §3 dưới)

PHẦN 1..6 — MỖI LAB MỘT CHƯƠNG (theo mẫu 11 mục §2)

PHẦN 7 — TỔNG KẾT & SO SÁNH CHÉO
  7.1 Cross-platform (Windows vs Linux) tổng hợp
  7.2 So sánh Lab 5 (classical) vs Lab 6 (PQC)
  7.3 Lessons learned toàn môn

APPENDIX A — Academic Integrity & Ethics (bắt buộc, §5)
APPENDIX B — Rubric tự chấm (bắt buộc, §6)
APPENDIX C — CLI usage examples, build logs, KAT coverage, unit test summary
TÀI LIỆU THAM KHẢO
```

---

## 2. Mẫu 11 mục cho MỖI lab (lặp lại cho Lab 1→6)

> Đây là khung mà mỗi file `labN_*.md` (mục 10 "Report Template") sẽ cụ thể hóa cho từng lab.

| # | Mục | Nội dung cần viết | Hình/Bảng/Biểu đồ kèm |
|---|-----|-------------------|------------------------|
| 1 | **Objectives** | Mục tiêu lab + bạn đã build gì & vì sao | — |
| 2 | **Environment** | Trỏ về bảng Hardware/OS chung; nêu version thư viện (Crypto++ x.y, OpenSSL z) | Bảng version deps |
| 3 | **System Design** | Kiến trúc (sơ đồ), class chính, lựa chọn tham số/mode, các misuse check | Sơ đồ kiến trúc (ASCII/vẽ) |
| 4 | **Implementation** | Điểm kỹ thuật cốt lõi, đoạn code quan trọng (ngắn), quyết định thiết kế | Ảnh chụp đoạn code/CLI |
| 5 | **KAT Validation** | Nguồn vector (cite NIST), số case, kết quả PASS/FAIL | Ảnh chụp `--kat` output + bảng coverage |
| 6 | **Negative Testing** | Các ca: wrong key, tampered ct, bad tag, malformed... + chứng minh fail-closed | Bảng ca test + ảnh chụp lỗi an toàn |
| 7 | **Performance Evaluation** | Áp dụng Performance Protocol; mean/median/stddev/95% CI | **Bảng số liệu + biểu đồ có error bar** |
| 8 | **Security Analysis** | Threat model, attack surface, misuse, known attacks, mitigation, limitations | Diagram (vd padding/length-extension) |
| 9 | **Lessons Learned** | Bug đã gặp & cách sửa, điều rút ra | — |
| 10 | **Conclusion** | Kết luận + hướng phát triển | — |
| 11 | **References** | NIST docs, RFC, tài liệu thư viện | — |

---

## 3. Performance Protocol (COPY nguyên văn vào mỗi lab)

> Đề yêu cầu mô tả phương pháp rõ ràng (không chỉ số thô). Đoạn dưới dán vào phần 0.3 và nhắc lại trong mỗi lab.

- **Bảng Hardware/OS:** CPU model, số core/thread, RAM, loại ổ lưu trữ, phiên bản OS, compiler & flags.
- **Warm-up:** 1–2 giây để ổn định cache/allocator; sau đó chạy 1000 vòng/block.
- **Runs:** **N ≥ 30** lần đo độc lập mỗi case.
- **Metrics:** mean, median, standard deviation, **95% Confidence Interval**; biểu đồ có **error bar**.
- **Fairness:** pin CPU governor (Linux `performance`; Windows power plan **High performance**).
- **Repeatability:** chỉ fix PRNG seed cho **dữ liệu synthetic**, **không bao giờ** cho key/nonce.

### Công thức 95% CI (ghi vào báo cáo để thể hiện hiểu thống kê)
Với n mẫu, trung bình mean, độ lệch chuẩn mẫu s:
```
95% CI = mean ± t(0.975, n-1) · s / √n
```
Với n ≥ 30 có thể xấp xỉ t ≈ 1.96.

### Format CSV chuẩn (mọi benchmark)
```csv
algo,mode,os,size_bytes,latency_ms_mean,latency_ms_median,latency_ms_stddev,ci95_ms,throughput_mb_s
AES-256,GCM,Windows,1048576,0.83,0.81,0.05,0.018,1262.7
```

---

## 4. Checklist ảnh/bảng/biểu đồ cần chụp (để không thiếu khi viết)

**Ảnh chụp màn hình (đặt trong `docs/screenshots/`):**
- [ ] Build thành công trên Windows (MSVC) — terminal/VS
- [ ] Build thành công trên Linux (GCC)
- [ ] `ctest` PASS cả 2 OS
- [ ] `--kat` runner in PASS/FAIL + summary (mỗi lab)
- [ ] Negative test: chương trình **từ chối an toàn** (tag fail, wrong key...)
- [ ] CLI usage tiêu biểu (encrypt/decrypt/sign/verify...)
- [ ] (Lab 4) Trang HTTPS chạy + chain of trust trong trình duyệt
- [ ] (Lab 4) MD5 collision: 2 file khác nhau, cùng digest

**Bảng số liệu:**
- [ ] Bảng version dependency
- [ ] Bảng Hardware/OS
- [ ] Bảng benchmark từng lab (mean/median/stddev/CI/throughput)
- [ ] Bảng KAT coverage (số case/PASS)
- [ ] Bảng negative test (ca / kỳ vọng / kết quả)
- [ ] Bảng rubric tự chấm (Appendix B)

**Biểu đồ (đặt trong `docs/plots/`):**
- [ ] Throughput (MB/s) vs payload size — có error bar
- [ ] Latency (ms/op) vs size
- [ ] So sánh Windows vs Linux
- [ ] So sánh mode/algorithm (vd AEAD vs non-AEAD; SHA-2 vs SHA-3; ECDSA vs RSA-PSS vs ML-DSA)

---

## 5. Appendix A — Academic Integrity & Ethics (dán nguyên văn)

> Đề yêu cầu đoạn này. Dịch/giữ song ngữ tùy bạn; ý phải đủ:

- Bài làm là của tôi; tôi cite thư viện, nguồn KAT và các tài liệu tham khảo.
- Mọi demo tấn công chỉ thực hiện trên **artifact của riêng tôi, trong sandbox offline**.
- Không test lên dữ liệu/mạng/dịch vụ của bên thứ ba.
- Mọi key/secret trong repo chỉ là **test keys**.
- Tôi hiểu khác biệt giữa **trình diễn lỗ hổng** và **khai thác ác ý**, và cam kết chỉ phục vụ học tập.
- (Nếu dùng AI/tool hỗ trợ) Tôi khai báo: …

---

## 6. Appendix B — Rubric tự chấm (điền trước khi nộp)

| Criterion | Points | Self-score | TA score |
|-----------|--------|-----------|----------|
| Correctness & KATs | 35–40 | | |
| Security hygiene / misuse checks | 10–20 | | |
| Cross-platform build & UX | 10 | | |
| Performance study quality | 10–20 | | |
| Report quality & clarity | 5–10 | | |
| Negative tests | 10–15 | | |
| Bonuses (if any) | + up to 15 | | |
| **Total** | **100 (+bonus)** | | |

> Mỗi lab còn có rubric **chi tiết riêng** — tự chấm theo bảng trong từng `labN_*.md` rồi tổng hợp vào đây.

---

## 7. Mẹo viết để được "Report quality" cao
- **Diễn giải, đừng dán số thô:** mỗi bảng/biểu đồ phải có 2–3 câu "kết quả này nói lên điều gì".
- **Liên kết theory ↔ engineering:** vd "GCM nhanh hơn CCM vì GHASH song song hóa tốt + AES-NI".
- **Trung thực:** nếu một test fail hay một mode chưa làm, **ghi rõ** trong Limitations — chấm điểm thích sự trung thực hơn là phóng đại.
- **Cite chuẩn:** FIPS-197, NIST SP 800-38A/38D, RFC 6979/8017, FIPS 203/204.
