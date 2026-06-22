// Lab 6 Benchmark — ML-DSA-44 + ML-KEM-512, with comparison table vs Lab 5
// Build: cmake --build ... --target lab6_bench
// Run:   lab6_bench [--large]

#include "pqtool.hpp"
#include <chrono>
#include <cstdio>
#include <vector>
#include <string>
#include <openssl/ml_kem.h>

using Clock = std::chrono::steady_clock;

static double elapsed_ms(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// ─── ML-DSA benchmark ────────────────────────────────────────────────────────
static void bench_mldsa(size_t msg_size, int reps) {
    std::vector<uint8_t> msg(msg_size, 0xAB);

    auto t0 = Clock::now();
    for (int i = 0; i < reps; i++) pq::mldsa_keygen();
    double kg_ms = elapsed_ms(t0) / reps;

    auto [priv, pub] = pq::mldsa_keygen();
    std::vector<uint8_t> sig;

    t0 = Clock::now();
    for (int i = 0; i < reps; i++) sig = pq::mldsa_sign(priv, msg);
    double sign_ms = elapsed_ms(t0) / reps;

    t0 = Clock::now();
    for (int i = 0; i < reps; i++) pq::mldsa_verify(pub, msg, sig);
    double verify_ms = elapsed_ms(t0) / reps;

    std::printf("ML-DSA-44       %8zu B  keygen %6.2f ms  sign %6.3f ms  verify %6.3f ms  reps=%d\n",
                msg_size, kg_ms, sign_ms, verify_ms, reps);
}

// ─── ML-KEM benchmark ────────────────────────────────────────────────────────
static void bench_mlkem(int reps) {
    auto t0 = Clock::now();
    for (int i = 0; i < reps; i++) pq::mlkem_keygen();
    double kg_ms = elapsed_ms(t0) / reps;

    auto [priv, pub] = pq::mlkem_keygen();
    pq::EncapsResult enc;

    t0 = Clock::now();
    for (int i = 0; i < reps; i++) enc = pq::mlkem_encaps(pub);
    double encaps_ms = elapsed_ms(t0) / reps;

    auto ct = enc.ciphertext;
    t0 = Clock::now();
    for (int i = 0; i < reps; i++) pq::mlkem_decaps(priv, ct);
    double decaps_ms = elapsed_ms(t0) / reps;

    std::printf("ML-KEM-512      %8s    keygen %6.2f ms  encaps %5.3f ms  decaps %5.3f ms  reps=%d\n",
                "N/A", kg_ms, encaps_ms, decaps_ms, reps);
}

int main(int argc, char** argv) {
    bool large = false;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--large") large = true;

    std::puts("=== Lab 6 Post-Quantum Benchmark ===\n");

    // ML-DSA at various message sizes
    std::puts("--- ML-DSA-44 (signature size: 2420 B) ---");
    bench_mldsa(   1024, 30);
    bench_mldsa(  16384, 20);
    bench_mldsa(1048576,  5);
    if (large) bench_mldsa(8*1048576, 3);

    std::putchar('\n');

    // ML-KEM (KEM, not a signature; msg size irrelevant)
    std::puts("--- ML-KEM-512 (ct: 768 B, ss: 32 B) ---");
    bench_mlkem(50);

    std::putchar('\n');
    std::puts("=== Comparison Table vs Lab 5 ===");
    std::puts("+-------------------+----------+----------+----------+-----------+");
    std::puts("| Scheme            | Sign/Enc | Ver/Dec  | Pubkey   | Sig/CT    |");
    std::puts("+-------------------+----------+----------+----------+-----------+");
    std::puts("| ECDSA-P256 (Lab5) | ~0.10 ms | ~0.10 ms | 64 B     | ~72 B     |");
    std::puts("| RSA-PSS-3072(Lab5)| ~2.0 ms  | ~0.2 ms  | 398 B    | 384 B     |");
    std::puts("| ML-DSA-44  (Lab6) | see above| see above| 1312 B   | 2420 B    |");
    std::puts("+-------------------+----------+----------+----------+-----------+");
    std::puts("| RSA-3072 KEM(Lab3)| ~2 ms    | ~30 ms   | 398 B    | 384 B     |");
    std::puts("| ML-KEM-512 (Lab6) | see above| see above| 800 B    | 768 B     |");
    std::puts("+-------------------+----------+----------+----------+-----------+");
    std::puts("\nNote: ML-DSA/ML-KEM sizes ~10–40× larger than classical → quantum resistance.");

    return 0;
}
