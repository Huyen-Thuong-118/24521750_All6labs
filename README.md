# 24521750 — Cryptography & Applications: All 6 Labs

**Sinh viên:** Nguyễn Đỗ Ngọc Huyền Thương — MSSV 24521750  
**Môn:** Mật mã học ứng dụng

Chuỗi 6 lab mật mã học: AES, RSA, Hash/PKI, Signatures, Post-Quantum, hiện thực bằng C++17 với CMake + vcpkg.

---

## Tổng Quan Các Lab

| Lab | Tool | Thư viện | Chủ đề |
|-----|------|----------|--------|
| Lab 1 | `aestool` | Crypto++ | AES 8 modes (ECB/CBC/OFB/CFB/CTR/XTS/CCM/GCM), AEAD, IV lifecycle |
| Lab 2 | `aestool2` | STL only | AES-128 thuần C++ (FIPS-197), CTR mode |
| Lab 3 | `rsatool` | Crypto++ | RSA-3072 OAEP(SHA-256), Hybrid AES-256-GCM |
| Lab 4 | `hashtool` | OpenSSL | SHA-2/SHA-3/SHAKE, X.509 parsing, length-extension attack |
| Lab 5 | `sigtool` | Crypto++ + OpenSSL | ECDSA-P256, RSA-PSS-3072 |
| Lab 6 | `pqtool` | OpenSSL 3.5+ | ML-DSA-44, ML-KEM-512, PQ certificates |

---

## Dependencies

| Thư viện / Công cụ | Version tối thiểu | Ghi chú |
|---|---|---|
| CMake | **3.20** | Bắt buộc, hỗ trợ presets |
| vcpkg | latest | Quản lý thư viện |
| MSVC (Windows) | Visual Studio **2022** (v143) | C++17, `/permissive-` |
| GCC (Linux) | **12.0** | hoặc Clang 15+ |
| Ninja (Linux) | **1.11** | Generator cho Linux build |
| **Crypto++** | **8.9** | via vcpkg: `cryptopp` — Labs 1, 2 (test only), 3, 5 |
| **OpenSSL** | **3.5.0** | via vcpkg: `openssl` — Labs 4, 5, 6 (ML-DSA/ML-KEM cần 3.5+) |
| **nlohmann-json** | **3.11** | via vcpkg: `nlohmann-json` — KAT runner, sidecar JSON |
| **Catch2** | **3.x** | via vcpkg: `catch2` — unit/integration tests |

> **Lưu ý Lab 6:** ML-DSA-44 và ML-KEM-512 yêu cầu **OpenSSL ≥ 3.5.0** (hỗ trợ native PQC). Phiên bản cũ hơn không compile được Lab 6.

---

## Cài Đặt

### Windows (VS2022)

```powershell
# 1. Cài Visual Studio 2022 với C++ Desktop Development workload
# 2. Cài CMake >= 3.20 (https://cmake.org/download/)
# 3. Cài vcpkg
git clone https://github.com/microsoft/vcpkg C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat

# 4. Set biến môi trường (thêm vào System Environment Variables)
$env:VCPKG_ROOT = "C:\vcpkg"

# 5. Clone project
git clone <repo-url> 24521750_All6labs
cd 24521750_All6labs
```

### Linux (Ubuntu 22.04 LTS hoặc 24.04 LTS)

```bash
# 1. Cài build tools
sudo apt-get update
sudo apt-get install -y build-essential git cmake ninja-build pkg-config \
     libssl-dev

# 2. Cài vcpkg
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT="$HOME/vcpkg"   # thêm vào ~/.bashrc

# 3. Clone project
git clone <repo-url> 24521750_All6labs
cd 24521750_All6labs
```

> **OpenSSL 3.5 trên Ubuntu:** nếu `apt` chưa có 3.5, dùng vcpkg sẽ tự build OpenSSL 3.5 từ source (mất ~5 phút).

---

## Build

### Cách 1 — Script nhanh

**Windows (PowerShell):**
```powershell
.\scripts\build_win.ps1           # Release + test
.\scripts\build_win.ps1 -Config Debug
```

**Linux (Bash):**
```bash
chmod +x scripts/build_linux.sh
./scripts/build_linux.sh
```

### Cách 2 — CMake Presets (khuyến nghị)

```bash
# Windows
cmake --preset win-msvc
cmake --build --preset win-msvc-release

# Linux
cmake --preset linux-gcc
cmake --build --preset linux-gcc-release
```

### Cách 3 — Manual

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release --parallel
```

### Chạy Tests

```bash
# Tất cả labs
ctest --test-dir build -C Release --output-on-failure

# Chỉ một lab
ctest --test-dir build -C Release -R "lab1" -V
ctest --test-dir build -C Release -R "lab2" -V
```

---

## Sử Dụng CLI

### Lab 1 — `aestool` (AES với Crypto++)

```bash
# Mã hóa file (CBC, AES-256)
aestool encrypt --mode cbc --key-hex <64-hex> --in plaintext.txt --out cipher.bin

# Mã hóa chuỗi text (GCM với AAD)
aestool encrypt --mode gcm --key-hex <64-hex> --text "Hello World" --aad "header" --encode hex

# Giải mã (đọc IV từ sidecar tự động)
aestool decrypt --mode gcm --key-hex <64-hex> --in cipher.bin --out plain.txt

# Chạy KAT (NIST test vectors)
aestool kat --file lab1_aes_cryptopp/kat/sp800-38a.json
aestool kat --file lab1_aes_cryptopp/kat/gcm.json
aestool kat --file lab1_aes_cryptopp/kat/ccm.json
```

### Lab 2 — `aestool2` (AES-128 thuần C++)

```bash
# Mã hóa CTR (hex key + hex IV, output hex)
aestool2 encrypt --key 2b7e151628aed2a6abf7158809cf4f3c \
                 --iv  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff \
                 --in plaintext.bin --out cipher.bin

# Giải mã
aestool2 decrypt --key 2b7e151628aed2a6abf7158809cf4f3c \
                 --iv  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff \
                 --in cipher.bin --out plain.bin

# Chạy KAT
aestool2 kat --file lab2_aes_manual/kat/fips197.json
aestool2 kat --file lab2_aes_manual/kat/sp800-38a-ctr.json
```

### Lab 3 — `rsatool` (RSA + Hybrid)

```bash
# Sinh key pair RSA-3072
rsatool genkey --out-priv priv.pem --out-pub pub.pem

# Mã hóa file (hybrid: RSA-OAEP wrap AES-256-GCM)
rsatool encrypt --pub pub.pem --in secret.txt --out envelope.bin

# Giải mã
rsatool decrypt --priv priv.pem --in envelope.bin --out secret.txt
```

### Lab 4 — `hashtool` (Hash + PKI)

```bash
# Hash file
hashtool hash --alg sha256 --in file.txt --encode hex
hashtool hash --alg sha3-256 --in file.txt
hashtool hash --alg shake128 --out-len 32 --in file.txt

# Parse X.509 certificate
hashtool cert --in certificate.pem --verbose

# Demo length-extension attack (offline, sandbox only)
hashtool length-ext --alg sha256 --msg "original" --mac <hex> --append "extra"
```

### Lab 5 — `sigtool` (Digital Signatures)

```bash
# Sinh key ECDSA-P256
sigtool genkey --alg ecdsa-p256 --out-priv priv.pem --out-pub pub.pem

# Ký
sigtool sign --alg ecdsa-p256 --priv priv.pem --in doc.pdf --out doc.sig

# Xác minh
sigtool verify --alg ecdsa-p256 --pub pub.pem --in doc.pdf --sig doc.sig
```

### Lab 6 — `pqtool` (Post-Quantum)

```bash
# ML-DSA-44 sign + verify
pqtool genkey --alg ml-dsa-44 --out-priv priv.key --out-pub pub.key
pqtool sign   --alg ml-dsa-44 --priv priv.key --in doc.txt --out doc.sig
pqtool verify --alg ml-dsa-44 --pub  pub.key  --in doc.txt --sig doc.sig

# ML-KEM-512 key encapsulation
pqtool kem-enc --alg ml-kem-512 --pub pub.key --out-ct ct.bin --out-ss ss.bin
pqtool kem-dec --alg ml-kem-512 --priv priv.key --in ct.bin --out-ss ss.bin
```

---

## Chạy Benchmark

```bash
# Lab 1 (8 mode × 6 size → CSV)
aes_bench --rounds 30 > lab1_bench.csv

# Lab 2 (AES-128 thuần C++ throughput)
aes2_bench > lab2_bench.csv

# Lab 3 (RSA keygen + encrypt/decrypt latency)
rsa_bench > lab3_bench.csv
```

---

## Cấu Trúc Thư Mục

```
24521750_All6labs/
├── README.md                   # File này
├── CMakeLists.txt              # Root CMake
├── CMakePresets.json           # Preset win-msvc / linux-gcc
├── vcpkg.json                  # Manifest dependencies
├── common/                     # Tiện ích dùng chung (CLI parser, codec, KAT runner...)
├── docs/
│   ├── guide/                  # Hướng dẫn lab (00_*.md, lab1..6_*.md)
│   └── report_labN.md          # Báo cáo từng lab
├── scripts/
│   ├── build_win.ps1           # Build script Windows
│   └── build_linux.sh          # Build script Linux
├── lab1_aes_cryptopp/          # Lab 1: src/ tests/ kat/ benchmark/
├── lab2_aes_manual/            # Lab 2: include/ src/ tests/ kat/ benchmark/
├── lab3_rsa_hybrid/            # Lab 3
├── lab4_hash_pki/              # Lab 4
├── lab5_signatures/            # Lab 5
└── lab6_post_quantum/          # Lab 6
```

---

## Known Limitations

### Lab 1
- Nonce-reuse detection chỉ là file-based local log — không chống reuse xuyên máy/process.
- Key lưu dạng raw test-key; chưa có KDF/passphrase derivation.
- Benchmark warm-up: 3 iterations (không đủ 1–2 giây theo khuyến nghị NIST).

### Lab 2
- Hiện thực table-based S-box → **không constant-time** (dễ bị cache-timing attack trên shared hardware).
- Chỉ hỗ trợ AES-128; AES-192/256 là bonus chưa implement.
- Không có authentication: CTR mode chỉ cung cấp confidentiality, không integrity.

### Lab 3
- RSA-3072 keygen chậm (~1–3 giây); không cache key.
- Chưa hỗ trợ certificate-based key exchange.

### Lab 4
- Length-extension attack demo chỉ chạy **offline, trên artifact của mình** — không test lên service bên ngoài.
- `cert_parser` hỗ trợ X.509 v3 cơ bản; một số extension phức tạp (OCSP stapling) chưa parse.

### Lab 5
- RFC 6979 deterministic nonce implemented; nhưng chưa có batch verification tối ưu.

### Lab 6
- **Yêu cầu OpenSSL ≥ 3.5.0.** Nếu hệ thống chỉ có OpenSSL 3.x < 3.5, Lab 6 sẽ không build được.
- ML-DSA/ML-KEM chưa hỗ trợ hybrid classical+PQ mode.

---

## Academic Integrity

Mọi code trong repo là tự viết bởi sinh viên MSSV 24521750. Các thư viện bên thứ ba (Crypto++, OpenSSL, nlohmann-json, Catch2) được dùng theo giấy phép tương ứng và được khai báo trong `vcpkg.json`. Test vectors lấy từ tài liệu công khai của NIST (FIPS-197, SP 800-38A, SP 800-38D, SP 800-38C). Công cụ AI hỗ trợ trong quá trình viết code (được khai báo theo quy định môn học).

---

## Self-Grade Checklist

| Criterion | Lab 1 | Lab 2 | Lab 3 | Lab 4 | Lab 5 | Lab 6 |
|---|---|---|---|---|---|---|
| Correctness & KATs | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Security hygiene / misuse checks | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Cross-platform build (CMake) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Unit tests (Catch2), ctest pass | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Negative tests | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Benchmark + CSV | ✅ | ⚠️ | ✅ | ✅ | ✅ | ✅ |
| Performance methodology (N≥30) | ✅ | ⚠️ | ✅ | ✅ | ✅ | ✅ |
| Report (11 mục) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| README top-level | ✅ | — | — | — | — | — |

> ⚠️ Lab 2 benchmark: N=5 reps (cần nâng lên N≥30 + mean/median/stddev/CI95).
