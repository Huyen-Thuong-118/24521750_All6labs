# Academic Integrity Declaration

**Student:** Nguyễn Đỗ Ngọc Huyền Thương  
**MSSV:** 24521750  
**Course:** Modern Applied Cryptography  
**Institution:** University of Information Technology — VNU-HCM  
**Submission date:** 2026-06-23

---

## Declaration

I declare that:

1. The work submitted is my own and was carried out in accordance with the university's academic integrity policy.

2. All code in this repository was written by me, except for third-party libraries listed below.

3. All test vectors and cryptographic constants are sourced from publicly available NIST standards and IETF RFCs, cited inline where used.

4. I have not submitted this work, or any substantial portion of it, for assessment in any other course or institution.

5. I understand that academic dishonesty, including plagiarism and unauthorized collaboration, may result in a grade of zero and further disciplinary action.

---

## Third-Party Libraries Used

| Library | Version | License | Purpose |
|---------|---------|---------|---------|
| Crypto++ | 8.9.0 | Boost Software License 1.0 | Lab 1: AES modes; Lab 5: ECDSA reference |
| OpenSSL | 3.x | Apache 2.0 | Lab 4: hash functions, X.509, EVP |
| liboqs | 0.10.x | MIT | Lab 6: ML-DSA-44, ML-KEM-512 |
| nlohmann/json | 3.11.x | MIT | JSON parsing for KAT files and PQ certs |
| Catch2 | 3.x | BSL-1.0 | Unit test framework (all labs) |

All libraries were obtained via **vcpkg** (Microsoft's open-source package manager) using the manifest file (`vcpkg.json`) included in the repository. No proprietary or unlicensed code was used.

---

## AI Tool Disclosure

In accordance with the course policy on AI-assisted work:

- Claude Code (Anthropic) was used as a coding assistant during development to explain cryptographic concepts, suggest test vectors, and help debug compilation errors.
- All code was reviewed, understood, and approved by me before inclusion. I can explain every function and design decision in this submission.
- AI assistance was used for productivity, not to substitute understanding of the subject matter.

---

**Signature (electronic):** Nguyễn Đỗ Ngọc Huyền Thương  
**Date:** 2026-06-23
