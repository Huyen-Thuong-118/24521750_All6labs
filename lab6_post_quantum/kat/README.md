# Lab 6 KAT Notes

NIST FIPS 204/203 do not publish full KAT input/output tables publicly yet
(only available via NIST submission packages). Instead, we use:

1. Parameter correctness (key/sig/ct sizes match FIPS spec)
2. Round-trip tests (keygen → sign → verify = true; tamper → false)
3. Cross-validation of ML-KEM shared secrets (encaps ss == decaps ss)

These are validated in `test_mldsa.cpp` and `test_mlkem.cpp` via Catch2.
