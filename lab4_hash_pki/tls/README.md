# Lab 4 — TLS Deployment

## Quick Start

### 1. Generate the self-signed certificate (already done)

```bash
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out tls/server.key
openssl req -new -x509 -key tls/server.key -out tls/server.crt -days 365 \
  -subj "/CN=localhost/O=Lab4-PKI-Demo/C=VN" \
  -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"
```

The generated files are committed:
- `server.key` — ECDSA P-256 private key
- `server.crt` — self-signed X.509 certificate (365 days, SAN: localhost)

### 2. Run nginx

```bash
# Copy certs to the path nginx.conf expects
sudo mkdir -p /etc/ssl/lab4
sudo cp tls/server.crt tls/server.key /etc/ssl/lab4/

# Run with the config file
sudo nginx -c $(pwd)/tls/nginx.conf

# Or, using the full path directly
sudo nginx -c /path/to/lab4_hash_pki/tls/nginx.conf
```

### 3. Test the TLS connection

```bash
# Accept self-signed cert with -k
curl -k https://localhost/
# Expected output: Lab4 TLS OK

# View the certificate details
echo | openssl s_client -connect localhost:443 -servername localhost 2>/dev/null \
  | openssl x509 -noout -text

# Or use verify_tls.sh:
bash tls/verify_tls.sh
```

## Trust Chain Explanation

```
[Browser/curl]
     |
     | Presents server.crt
     v
[server.crt] — self-signed (Subject == Issuer == CN=localhost)
     |
     | Signed by its OWN key (no CA)
     v
  [TRUST STORE] ← NOT in any public trust store
```

This is a **self-signed certificate** — there is no CA. The browser will show a warning.

In production, you would use:
- **Let's Encrypt** (free, automated via `certbot`, 90-day certs)
- **ZeroSSL** (free alternative to Let's Encrypt)
- A corporate CA if deploying on an internal network

The trust chain for a Let's Encrypt certificate would be:
```
server.crt  ← signed by Let's Encrypt R11 (intermediate CA)
R11.crt     ← signed by ISRG Root X1 (root CA, in all browsers)
```

## Why ECDSA over RSA?

| Property | RSA-2048 | ECDSA-P256 |
|----------|----------|------------|
| Security level | ~112 bits | ~128 bits |
| Key size | 2048 bits | 256 bits |
| Signature size | 256 bytes | ~72 bytes |
| Handshake cost (sign) | ~1 ms | ~0.1 ms |
| Forward secrecy | needs DHE | needs ECDHE |

ECDSA-P256 provides **stronger security in a smaller key** — the private key is 256 bits vs 2048 bits for RSA at a lower security level. The smaller signature also reduces TLS handshake bandwidth.

Both RSA and ECDSA require a separate key-exchange step (DHE/ECDHE) for **perfect forward secrecy** (PFS) — even if the server's long-term key is compromised later, recorded past sessions cannot be decrypted.

## TLS 1.3 vs TLS 1.2

| Feature | TLS 1.2 | TLS 1.3 |
|---------|---------|---------|
| Round trips (full handshake) | 2-RTT | 1-RTT |
| 0-RTT resumption | No | Yes (with risk) |
| Key exchange | RSA or (EC)DHE | ECDHE only (always PFS) |
| Cipher suites | Negotiated (many options) | Fixed 5 AEAD suites |
| Record MAC | Separate (vulnerable to padding attacks) | Integrated with AEAD |
| Session tickets | Optional | Mandatory for 0-RTT |

TLS 1.3 removes all insecure options (RC4, 3DES, MD5, SHA-1, RSA key exchange, CBC mode) from the protocol — a client and server that both support TLS 1.3 cannot accidentally negotiate a weak cipher suite.
