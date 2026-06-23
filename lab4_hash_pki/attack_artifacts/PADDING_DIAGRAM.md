# SHA-256 Length-Extension Attack — Padding Diagram

## Background

SHA-256 processes input in 512-bit (64-byte) blocks. Before hashing, the message is
padded with:

1. A single `0x80` byte
2. Zero bytes until the total length is `64k - 8` bytes for some integer `k`
3. The original message bit-length as a big-endian 64-bit integer

The internal state after processing a block is the intermediate hash value (IHV), which
is **exactly the SHA-256 digest** if that block was the last one.

## The Scenario

```
Secret key:   "secretkey"         (9 bytes, unknown to attacker)
Message:      "amount=100&user=alice"  (21 bytes, known to attacker)
Observed MAC: SHA256(key || msg)   (32 bytes, sent by server)
```

## Step 1: What the Server Computes

The server concatenates key and message, then SHA-256-pads the 30 bytes (9+21):

```
Byte offset   Content
──────────────────────────────────────────────────────────────────
00-08         s e c r e t k e y          ← secret key (9 bytes)
09-29         a m o u n t = 1 0 0        ← message (21 bytes)
              & u s e r = a l i c e
──────── total 30 bytes, padded to 64 bytes ─────────────────────
1E            0x80                        ← padding start bit
1F-37         0x00 × 24                   ← zero padding
38-3F         0x00 0x00 0x00 0x00         ← message bit-length
              0x00 0x00 0x00 0xF0         ← 240 bits = 30 bytes × 8
──────────────────────────────────────────────────────────────────
```

SHA-256 processes this single 64-byte block and outputs a 32-byte **intermediate state**:

```
SHA256(key || msg) = 4670e37dd185f6b845411c8d429b4fe1d7a9b0d0f92cb223abbc54e100eb6ac6
```

This MAC is sent to the client alongside the message.

## Step 2: The Length-Extension Property

The SHA-256 output IS the 256-bit internal chaining value after the last block.
An attacker who observes the MAC can **reinitialize SHA-256 with that state** and
continue feeding more data — without knowing the secret key.

The forged message the attacker constructs is:

```
Byte offset   Content
──────────────────────────────────────────────────────────────────
00-1D         amount=100&user=alice      ← original message (visible)
1E            0x80                        ← SHA-256 padding of (key||msg)
1F-37         0x00 × 24                   ← zero padding
38-3F         0x00 0x00 0x00 0x00
              0x00 0x00 0x00 0xF0         ← original bit-length: 240
──────── second SHA-256 block starts here ──────────────────────
3E-49         & a d m i n = t r u e      ← appended extension
4A            0x80                        ← padding for second block
4B-77         0x00 × 45                   ← zero padding
78-7F         0x00 0x00 0x00 0x00
              0x00 0x00 0x02 0x50         ← total bit-length: 592 bits
──────────────────────────────────────────────────────────────────
```

The attacker initializes SHA-256 with the observed MAC as the initial state, then
feeds the extension block. The result is:

```
Forged MAC = 6376aa71c482a2c57ab4a8ca5b0a0dfd05b177e2a1c40af7b219736d14ee4d21
```

## Step 3: Server Verification

The server receives the forged message (without the key prefix) and computes:

```
SHA256("secretkey" || forged_message)
```

Because `"secretkey" || forged_message` has the same structure as what the attacker
fed into the continued SHA-256 computation, the server's result equals the forged MAC
exactly:

```
SHA256(key || forged_msg) = 6376aa71c482a2c57ab4a8ca5b0a0dfd05b177e2a1c40af7b219736d14ee4d21
                          = Forged MAC  ✓  ACCEPTED
```

## Why HMAC Defeats This Attack

HMAC-SHA256 computes:

```
HMAC-SHA256(k, m) = SHA256( (k XOR opad) || SHA256( (k XOR ipad) || m ) )
```

The outer SHA-256 call takes the inner hash as input — **not** as its initial state.
An attacker cannot initialize the outer SHA-256 with the HMAC output and continue
feeding data, because the output is the result of a complete, finalized hash of
`(k XOR opad) || inner_hash`. Any extension would need to know `(k XOR opad)`, which
requires knowing `k`.

```
Naive MAC:  SHA256(k || m)           ← vulnerable (state exposed as output)
HMAC:       SHA256(k⊕opad || SHA256(k⊕ipad || m))  ← secure
```

## Attack Preconditions

For the length-extension attack to succeed, the attacker needs to know:
1. The MAC value (observes from network)
2. The original message (observes from network)
3. The key LENGTH (can often be guessed or brute-forced for small keys)

The attacker does **not** need to know the key VALUE.

## Affected Primitives

| Hash | Vulnerable to length extension? |
|------|--------------------------------|
| MD5  | Yes |
| SHA-1 | Yes |
| SHA-256 | Yes |
| SHA-512 | Yes |
| SHA-3/Keccak | **No** (sponge construction) |
| BLAKE2/BLAKE3 | **No** (tree hashing mode) |
| HMAC-SHA256 | **No** (double-wrapping) |
