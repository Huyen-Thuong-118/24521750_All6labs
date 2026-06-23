# MD5 Collision Demonstration

## Digest Match

Both `file1.bin` and `file2.bin` (128 bytes each) produce the same MD5 hash:

```
79054025255fb1a26e4bc422aef54eb4
```

The files differ in exactly **6 bits** (bit 7 at byte offsets 0x13, 0x2d, 0x3b, 0x53, 0x6d, 0x7b).

## Why MD5 Is Broken

### 1. Birthday Bound — Structural Weakness

A hash function with `n`-bit output has a birthday-attack collision resistance of only `2^(n/2)` operations — regardless of design quality. MD5 produces 128-bit digests, so the birthday bound is `2^64 ≈ 1.8 × 10^19` operations. While once considered acceptable (1990s hardware), `2^64` operations are feasible with modern GPU clusters within days.

However, MD5 is far worse than its birthday bound:

### 2. Wang et al. (2004/2005) — Differential Cryptanalysis

Xiaoyun Wang and Hongbo Yu showed in _"How to Break MD5 and Other Hash Functions"_ (EUROCRYPT 2005) that MD5 collisions can be found using **differential cryptanalysis** in fewer than **2^24** MD5 compressions — roughly one second on a modern CPU.

The attack exploits a **differential path**: a carefully crafted XOR difference `dM = M' XOR M` that, when two messages differing by `dM` are fed through the MD5 compression function, produces the same internal state with very high probability. The probability amplification comes from careful analysis of how carry bits propagate through MD5's arithmetic operations.

The collision pair in `file1.bin`/`file2.bin` uses the Wang 2005 two-block differential with 6 controlled bit flips spread across both 64-byte message blocks.

### 3. Collision vs. Preimage — A Critical Distinction

| Attack type | Goal | MD5 cost | SHA-256 cost |
|-------------|------|----------|--------------|
| **Collision** | Find any M, M' with H(M)=H(M') | < 2^24 (Wang) | ~2^128 (birthday) |
| **2nd preimage** | Given M, find M'≠M with same hash | ~2^123 | ~2^255 |
| **Preimage** | Given H, find any M with H(M)=H | ~2^123 | ~2^255 |

**Collisions are the cheapest attack.** An attacker does NOT need to reverse the hash — they only need to find two inputs that land in the same bucket. Code-signing and certificate fingerprinting require collision resistance; an MD5-signed document can be swapped for a malicious one with the same hash.

### 4. Real-World Incidents

**Flame Malware (2012)**: The Flame cyberweapon (attributed to US/Israeli intelligence) forged a valid Windows Update certificate by exploiting an MD5 chosen-prefix collision. The attack produced a rogue CA certificate that appeared signed by Microsoft's root CA. This allowed Flame to distribute itself through Windows Update infrastructure. Full technical analysis published by CWI Amsterdam and Marc Stevens.

**Fraudulent TLS Certificates (2008)**: Sotirov et al. used a chosen-prefix MD5 collision to create a rogue CA certificate signed by RapidSSL. They purchased legitimate end-entity certificates, exploited MD5 to produce a CA certificate with the same hash, and showed they could impersonate any HTTPS site.

**HashClash / MD5 Tunnels**: Chosen-prefix collision attacks (Mark Stevens, 2007) allow constructing two meaningful documents (e.g., two PDF files with different content) with the same MD5, at a cost of ~2^49 operations — feasible with a botnet or cloud compute cluster within hours.

### 5. Why Collisions Matter in Practice

Same hash ≠ same content. Specific systems that break when MD5 collisions are possible:

- **Code signing**: Malware can be bundled with a legitimate program to produce the same MD5 installer hash
- **Certificate fingerprints**: CAs using MD5 for cross-signing can be exploited to issue rogue sub-CAs
- **Software package managers**: Package checksums in `.deb`/`.rpm` repos relying on MD5 are forgeable
- **Digital signatures**: Signing `H(document)` under MD5 allows swapping the document post-signature

## Mitigation

1. **SHA-256 minimum**: 2^128 collision resistance (birthday), no known structural attacks. All code signing, TLS certificates, and checksums should use SHA-256 or better.

2. **SHA-3 for new protocols**: Keccak sponge construction provides a fundamentally different security model; not vulnerable to length-extension or differential attacks that affect SHA-2.

3. **CA/Browser Forum ban**: MD5 prohibited in publicly trusted certificates since 2014 (Baseline Requirements §7.1.3). All major browsers reject MD5 in TLS chains.

4. **Algorithm agility**: Systems must support upgrading hash algorithms without breaking backward compatibility. Hard-coding MD5 is an architectural vulnerability even if current attacks are "impractical" for a specific use case.

5. **HMAC mitigation**: MAC-based schemes should use `HMAC-SHA256`. HMAC wraps the hash as `H(k XOR opad || H(k XOR ipad || m))`, preventing both length-extension and collision attacks from forging MACs.

## References

- Wang, X., Yu, H.: "How to Break MD5 and Other Hash Functions." EUROCRYPT 2005.
- Sotirov et al.: "MD5 Considered Harmful Today." 25C3, 2008.
- Stevens, M.: "On Collisions for MD5." MSc thesis, Eindhoven, 2007.
- Mikle, O.: "Practical Attacks on Digital Signatures Using MD5 Message Digest Protocol." Cryptology ePrint 2004.
