#!/usr/bin/env bash
# verify_attack.sh — SHA-256 Length-Extension Attack Demonstration
# Requires: hashtool in PATH (or set HASHTOOL=<path>)
#
# Scenario:
#   A server computes MAC = SHA256("secretkey" || "amount=100&user=alice")
#   and sends it alongside the message to the client.
#   An attacker (who knows the MAC and message length, but NOT the key)
#   can forge a valid MAC for the message extended with "&admin=true".

HASHTOOL="${HASHTOOL:-hashtool}"

KEY="secretkey"
MSG="amount=100&user=alice"
EXT="&admin=true"
KEYLEN=9

KEY_HEX=$(printf '%s' "$KEY" | xxd -p | tr -d '\n')
MSG_HEX=$(printf '%s' "$MSG" | xxd -p | tr -d '\n')
EXT_HEX=$(printf '%s' "$EXT" | xxd -p | tr -d '\n')

echo "=== SHA-256 Length-Extension Attack Demo ==="
echo ""
echo "Key:     $KEY  ($KEYLEN bytes)"
echo "Message: $MSG"
echo ""

# Step 1: Server computes MAC = SHA256(key || msg)
MAC=$("$HASHTOOL" hash --algo sha256 --hex "${KEY_HEX}${MSG_HEX}")
echo "Step 1 — Legitimate MAC (server-side):"
echo "  SHA256(key || msg) = $MAC"
echo ""

# Step 2: Attacker runs extend WITHOUT knowing the key
echo "Step 2 — Attacker forges MAC for '$EXT' extension:"
RESULT=$("$HASHTOOL" extend \
    --hash   "$MAC"     \
    --keylen "$KEYLEN"  \
    --msg    "$MSG_HEX" \
    --ext    "$EXT_HEX")
echo "$RESULT" | sed 's/^/  /'

FORGED_HASH=$(echo "$RESULT" | grep "Forged hash:" | awk '{print $3}')
FORGED_MSG_HEX=$(echo "$RESULT" | grep "Forged message:" | awk '{print $3}')
echo ""

# Step 3: Verify the forgery — server must accept it
echo "Step 3 — Server verification of forged request:"
VERIFY=$("$HASHTOOL" hash --algo sha256 --hex "${KEY_HEX}${FORGED_MSG_HEX}")
if [ "$VERIFY" = "$FORGED_HASH" ]; then
    echo "  SHA256(key || forged_msg) = $VERIFY"
    echo "  RESULT: FORGERY ACCEPTED — attacker wins!"
else
    echo "  ERROR: Forgery check failed (this should not happen)"
    exit 1
fi

echo ""
echo "=== Attack Summary ==="
echo "  The attacker learned the MAC of 'amount=100&user=alice'"
echo "  and WITHOUT the key, produced a valid MAC for a message"
echo "  with '&admin=true' appended (after SHA-256 padding bytes)."
echo "  The server's SHA256(key || forged_msg) equals the forged MAC."
echo ""
echo "  Fix: use HMAC-SHA256 instead of SHA256(key || msg)"
