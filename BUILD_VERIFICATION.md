# Build Verification

**Date:** 2026-06-23  
**Platform:** Windows 11 (10.0.26200), MSVC via MSBuild  
**Build system:** CMake + vcpkg (manifest mode)  
**Configuration:** Release

## Build Command

```powershell
cmake -S . -B build/win -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
cmake --build build/win --config Release
```

## Test Results

```
ctest -C Release --timeout 120
```

```
100% tests passed, 0 tests failed out of 273

Total Test time (real) = 18.02 sec
```

## Executables Built

| Target | Binary | Lab |
|--------|--------|-----|
| `aestool` | `lab1_aes_cryptopp/Release/aestool.exe` | Lab 1 |
| `lab1_tests` | `lab1_aes_cryptopp/Release/lab1_tests.exe` | Lab 1 |
| `lab2_aestool` | `lab2_aes_manual/Release/lab2_aestool.exe` | Lab 2 |
| `lab2_tests` | `lab2_aes_manual/Release/lab2_tests.exe` | Lab 2 |
| `hashtool` | `lab4_hash_pki/Release/hashtool.exe` | Lab 4 |
| `lab4_tests` | `lab4_hash_pki/Release/lab4_tests.exe` | Lab 4 |
| `gen_collision` | `lab4_hash_pki/Release/gen_collision.exe` | Lab 4 |
| `sigtool` | `lab5_signatures/Release/sigtool.exe` | Lab 5 |
| `lab5_tests` | `lab5_signatures/Release/lab5_tests.exe` | Lab 5 |
| `pqtool` | `lab6_post_quantum/Release/pqtool.exe` | Lab 6 |
| `lab6_tests` | `lab6_post_quantum/Release/lab6_tests.exe` | Lab 6 |

## Key Attack Demos Verified

### MD5 Collision (Wang et al. 2005)
```
./gen_collision.exe lab4_hash_pki/attacks/md5_collision/
MD5 Collision Demonstration (Wang et al. EUROCRYPT 2005)
Message 1 (128 bytes): 79054025255fb1a26e4bc422aef54eb4
Message 2 (128 bytes): 79054025255fb1a26e4bc422aef54eb4
Expected:              79054025255fb1a26e4bc422aef54eb4
Collision: CONFIRMED
```

### SHA-256 Length-Extension Attack
```
# Initial MAC = SHA256("secretkey" || "amount=100&user=alice")
MAC = 4670e37dd185f6b845411c8d429b4fe1d7a9b0d0f92cb223abbc54e100eb6ac6

# Forged MAC (attacker adds "&admin=true" without knowing the key)
./hashtool.exe extend --hash 4670e37d... --keylen 9 --msg 616d6f756e74... --ext 2661646d696e...
Forged hash:    6376aa71c482a2c57ab4a8ca5b0a0dfd05b177e2a1c40af7b219736d14ee4d21
Forged message: 616d6f756e743d31303026...802661646d696e3d74727565

# Verification: SHA256(key || forged_msg) == Forged hash
SHA256(key || forged_msg): 6376aa71c482a2c57ab4a8ca5b0a0dfd05b177e2a1c40af7b219736d14ee4d21
ATTACK CONFIRMED
```
