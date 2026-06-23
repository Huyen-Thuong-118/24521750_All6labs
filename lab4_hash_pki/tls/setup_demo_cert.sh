#!/usr/bin/env bash
# setup_demo_cert.sh — Generate a 3-tier PKI for the Lab 4 TLS demo
#
# Creates:
#   certs/root_ca.key   + root_ca.pem     (self-signed Root CA, 10 years)
#   certs/sub_ca.key    + sub_ca.pem      (intermediate CA, signed by root, 5 years)
#   certs/server.key    + server.pem      (end-entity cert, signed by sub_ca, 1 year)
#   certs/server_chain.pem               (server + sub_ca bundle for nginx)
#   certs/ca_chain.pem                   (sub_ca + root for OCSP stapling)
#
# Requirements: openssl >= 1.1.1
# Usage:        bash setup_demo_cert.sh [output_dir]

set -euo pipefail
OUTDIR="${1:-./certs}"
mkdir -p "$OUTDIR"

DAYS_ROOT=3650   # 10 years
DAYS_SUBCA=1825  # 5 years
DAYS_SERVER=365  # 1 year

echo "[*] Generating Root CA ..."
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out "$OUTDIR/root_ca.key"
openssl req -new -x509 -days "$DAYS_ROOT" \
    -key "$OUTDIR/root_ca.key" \
    -out "$OUTDIR/root_ca.pem" \
    -subj "/C=VN/ST=HCM/O=Lab4 Demo/CN=Lab4 Root CA" \
    -addext "basicConstraints=critical,CA:TRUE,pathlen:1" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" \
    -addext "subjectKeyIdentifier=hash"

echo "[*] Generating Intermediate (Sub) CA ..."
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out "$OUTDIR/sub_ca.key"
openssl req -new \
    -key "$OUTDIR/sub_ca.key" \
    -out "$OUTDIR/sub_ca.csr" \
    -subj "/C=VN/ST=HCM/O=Lab4 Demo/CN=Lab4 Intermediate CA"

openssl x509 -req -days "$DAYS_SUBCA" \
    -in  "$OUTDIR/sub_ca.csr" \
    -CA  "$OUTDIR/root_ca.pem" \
    -CAkey "$OUTDIR/root_ca.key" \
    -CAcreateserial \
    -out "$OUTDIR/sub_ca.pem" \
    -extfile <(printf "basicConstraints=critical,CA:TRUE,pathlen:0\nkeyUsage=critical,keyCertSign,cRLSign\nsubjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid,issuer\n")

echo "[*] Generating Server certificate ..."
openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -out "$OUTDIR/server.key"
openssl req -new \
    -key "$OUTDIR/server.key" \
    -out "$OUTDIR/server.csr" \
    -subj "/C=VN/ST=HCM/O=Lab4 Demo/CN=lab4.local"

openssl x509 -req -days "$DAYS_SERVER" \
    -in  "$OUTDIR/server.csr" \
    -CA  "$OUTDIR/sub_ca.pem" \
    -CAkey "$OUTDIR/sub_ca.key" \
    -CAcreateserial \
    -out "$OUTDIR/server.pem" \
    -extfile <(printf "basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:lab4.local,DNS:localhost,IP:127.0.0.1\nsubjectKeyIdentifier=hash\nauthorityKeyIdentifier=keyid,issuer\n")

echo "[*] Building certificate bundles ..."
cat "$OUTDIR/server.pem" "$OUTDIR/sub_ca.pem" > "$OUTDIR/server_chain.pem"
cat "$OUTDIR/sub_ca.pem" "$OUTDIR/root_ca.pem" > "$OUTDIR/ca_chain.pem"

echo "[*] Verifying chain ..."
openssl verify -CAfile "$OUTDIR/root_ca.pem" -untrusted "$OUTDIR/sub_ca.pem" "$OUTDIR/server.pem"

echo ""
echo "=== Certificate Summary ==="
for pem in root_ca.pem sub_ca.pem server.pem; do
    echo ""
    echo "--- $pem ---"
    openssl x509 -in "$OUTDIR/$pem" -noout \
        -subject -issuer -dates \
        -fingerprint -sha256
done

echo ""
echo "[OK] Certificates written to $OUTDIR/"
echo "     Copy server_chain.pem and server.key to /etc/ssl/lab4/ for nginx."
echo "     Add root_ca.pem to your system/browser trust store."
