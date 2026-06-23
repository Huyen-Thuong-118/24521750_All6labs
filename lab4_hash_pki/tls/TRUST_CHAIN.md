# TLS Trust Chain — Lab 4 PKI Demo

## Certificate Hierarchy

```
Root CA  (self-signed, pathlen:1)
  └── Intermediate CA  (signed by Root, pathlen:0)
        └── Server Certificate  (signed by Intermediate)
```

Run `setup_demo_cert.sh` to generate all three tiers using P-256 ECDSA keys:

```bash
bash tls/setup_demo_cert.sh tls/certs
```

## Files Generated

| File | Role | Signed by | Validity |
|------|------|-----------|---------|
| `root_ca.key` / `root_ca.pem` | Root CA | self | 10 years |
| `sub_ca.key` / `sub_ca.pem` | Intermediate CA | Root CA | 5 years |
| `server.key` / `server.pem` | End-entity (TLS server) | Intermediate CA | 1 year |
| `server_chain.pem` | nginx `ssl_certificate` | — | bundle |
| `ca_chain.pem` | nginx `ssl_trusted_certificate` | — | bundle |

## How Chain Verification Works

TLS handshake validation follows the X.509 path-building algorithm:

1. **Server sends**: `server.pem` + `sub_ca.pem` (the chain bundle)
2. **Client looks up**: Root CA from its local trust store
3. **Client verifies**:
   - `server.pem` signature with `sub_ca.pem`'s public key ✓
   - `sub_ca.pem` signature with `root_ca.pem`'s public key ✓
   - `root_ca.pem` is in the trust store ✓
4. **Additional checks**: validity period, SAN match, basicConstraints, keyUsage

```
Client                                  Server
  |  ── ClientHello (TLS 1.3) ────────>  |
  |  <─ ServerHello + Certificate ──────  |  (sends server_chain.pem)
  |  <─ CertificateVerify ──────────────  |  (ECDSA-P256 signature)
  |  <─ Finished ───────────────────────  |
  |                                       |
  |  Verifies: leaf ← subCA ← rootCA     |  (path validation)
  |  Checks:   SAN, keyUsage, dates       |
  |  ── Finished ───────────────────────> |
  |  Application data (encrypted) ......  |
```

## basicConstraints and Path Length

The `pathlen` constraint limits how many more CA certificates can appear below a CA:

- **Root CA**: `pathlen:1` — allows one intermediate below it
- **Intermediate CA**: `pathlen:0` — cannot sign further CAs, only end-entity certs
- **Server cert**: `CA:FALSE` — cannot sign any certificates at all

Violating `pathlen` causes chain validation to fail. This prevents a compromised
intermediate CA from minting another intermediate and extending its attack surface.

## Key Usage and Extended Key Usage

| Certificate | keyUsage | extendedKeyUsage |
|------------|----------|------------------|
| Root CA | `keyCertSign`, `cRLSign` | — |
| Intermediate CA | `keyCertSign`, `cRLSign` | — |
| Server | `digitalSignature` | `serverAuth` |

A server certificate with `keyCertSign` set would be a misconfiguration — it could be
used to sign other certificates. Modern clients reject such chains.

## Why ECDSA-P256 Instead of RSA?

| Property | RSA-2048 | ECDSA-P256 |
|----------|----------|------------|
| Security level | ~112 bits | ~128 bits |
| Key size | 2048 bits | 256 bits |
| Signature size | 256 bytes | ~72 bytes |
| Handshake cost | ~1 ms (sign) | ~0.15 ms |
| Forward secrecy | needs DHE | needs ECDHE |

ECDSA-P256 provides stronger security in a smaller key. Combined with ECDHE for key
exchange (so the session key is ephemeral), TLS 1.3 with ECDSA achieves **perfect
forward secrecy**: even if the server's long-term key is later compromised, past
sessions cannot be decrypted.

## Why SHA-256 in Certificates? (Not MD5/SHA-1)

The signature algorithm in these certificates is `ecdsa-with-SHA256`. Historical context:

- **MD5 certificates** (pre-2009): Flame malware forged a Microsoft Windows Update CA
  using an MD5 chosen-prefix collision. MD5 banned from publicly trusted CAs in 2011.
- **SHA-1 certificates** (pre-2016): Google demonstrated a SHA-1 collision (SHAttered,
  2017). SHA-1 banned from CA/Browser Forum Baseline Requirements in 2016.
- **SHA-256 certificates** (current): No known collision attacks. Required for all
  publicly trusted certificates since 2016.

## Testing the Setup

After running `setup_demo_cert.sh`:

```bash
# Verify the chain
openssl verify -CAfile certs/root_ca.pem -untrusted certs/sub_ca.pem certs/server.pem

# Inspect server cert
openssl x509 -in certs/server.pem -noout -text | grep -E "Subject:|Issuer:|Not|SAN|Key Usage"

# Test nginx (after copying certs and editing nginx_tls.conf)
nginx -t
curl -v --cacert certs/root_ca.pem https://lab4.local/
```
