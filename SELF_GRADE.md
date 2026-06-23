# Self-Grade Sheet

**Student:** Nguyễn Đỗ Ngọc Huyền Thương  
**MSSV:** 24521750  
**Course:** Modern Applied Cryptography  
**Date:** 2026-06-23

---

## Lab 1 — AES with Crypto++ (Symmetric Encryption)

| Criterion | Max | Self | Notes |
|-----------|-----|------|-------|
| ECB/CBC/OFB/CFB/CTR/XTS modes implemented | 20 | 20 | All 6 modes via Crypto++ |
| GCM / CCM authenticated encryption | 15 | 15 | Tag verification tested |
| SP 800-38A KAT vectors (F.2–F.5) | 10 | 10 | 13 vectors in sp800-38a.json |
| Negative tests (wrong key/tag/IV) | 10 | 10 | 22 test cases |
| Build passes, 0 test failures | 5 | 5 | 273/273 pass |
| **Subtotal** | **60** | **60** | |

## Lab 2 — AES-128 Pure C++17 Implementation

| Criterion | Max | Self | Notes |
|-----------|-----|------|-------|
| SubBytes, ShiftRows, MixColumns, AddRoundKey | 20 | 20 | Verified vs FIPS-197 App B/C |
| KeyExpansion (10 rounds) | 10 | 10 | rk1, rk2, rk10 match App A.1 |
| CTR mode wrapper | 10 | 10 | F.5.1 all 4 blocks pass |
| Cross-validation vs Crypto++ | 10 | 10 | 5 random-key × 5 random-msg |
| GF(2^8) arithmetic (xtime, gf_mul) | 5 | 5 | §4.2 example verified |
| S-box correctness (INV_SBOX round-trip) | 5 | 5 | All 256 values tested |
| **Subtotal** | **60** | **60** | |

## Lab 3 — (not assigned / combined with Lab 2)

Not applicable for this semester.

## Lab 4 — Hashing, PKI & Practical Attacks

| Criterion | Max | Self | Notes |
|-----------|-----|------|-------|
| SHA-224/256/384/512, SHA-3, SHAKE hash KATs | 15 | 15 | sha2/sha3/shake JSON vectors |
| X.509 certificate parsing & chain verification | 15 | 14 | DER/PEM parsing; chain verify works but limited to 2-tier |
| SHA-256 length-extension attack | 15 | 15 | Forge MAC without knowing key |
| MD5 collision demonstration (Wang et al.) | 10 | 10 | 128-byte pair, verified identical MD5 |
| TLS configuration & PKI setup scripts | 5 | 5 | 3-tier cert chain, nginx config |
| **Subtotal** | **60** | **59** | |

## Lab 5 — Digital Signatures (ECDSA-P256)

| Criterion | Max | Self | Notes |
|-----------|-----|------|-------|
| ECDSA sign/verify with P-256 | 20 | 20 | OpenSSL EVP via Crypto++ |
| RFC 6979 deterministic nonce | 15 | 15 | Same msg+key = same sig |
| Negative tests (tampered sig/hash) | 10 | 10 | Reject all tampered inputs |
| **Subtotal** | **45** | **45** | |

## Lab 6 — Post-Quantum Cryptography (ML-DSA / ML-KEM)

| Criterion | Max | Self | Notes |
|-----------|-----|------|-------|
| ML-DSA-44 keygen / sign / verify | 20 | 20 | Via liboqs |
| ML-KEM-512 keygen / encap / decap | 20 | 20 | Shared secret matches |
| PQ Certificate (issue / verify / tamper) | 15 | 15 | JSON cert with ML-DSA sig |
| **Subtotal** | **55** | **55** | |

---

## Total

| | Max | Self |
|---|-----|------|
| All 6 labs | 280 | 279 |

---

## Known Limitations

- Lab 4 X.509 chain verification: the implementation correctly verifies issuer/subject/signature linkage but does not check CRL/OCSP revocation status.
- Performance benchmarks were not run — benchmark executables exist but no formal timing data was collected.
- Lab 4 TLS configuration is a reference config (nginx_tls.conf); a live TLS server was not deployed as part of the submission.
