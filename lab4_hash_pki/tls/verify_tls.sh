#!/bin/bash
# verify_tls.sh — Quick TLS certificate inspection
# Usage: bash verify_tls.sh [host] [port]
# Default: localhost:443

HOST="${1:-localhost}"
PORT="${2:-443}"

echo "=== TLS Certificate Inspection: $HOST:$PORT ==="
echo ""

echo | openssl s_client -connect "${HOST}:${PORT}" -servername "${HOST}" 2>/dev/null \
  | openssl x509 -noout -text \
  | grep -E "Subject:|Issuer:|Not After|Signature Algorithm:|Public-Key|DNS:|IP Address:"
