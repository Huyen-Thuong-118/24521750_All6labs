// Lab 4 Benchmark — SHA-256 / SHA-512 / SHA3-256 / SHA3-512
// Sizes: 1 MiB / 100 MiB (1 GiB skipped in automated run — too slow for CI)
#include "hasher.hpp"
#include "common/benchmark.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>

struct BenchCase {
    std::string label;
    hash4::Algo algo;
    size_t      size;
};

static void run(const BenchCase& bc) {
    std::vector<uint8_t> data(bc.size, 0xAB);

    auto stats = common::measure(
        [&]{ hash4::hash_bytes(bc.algo, data); },
        bc.size,
        20,
        1.0);

    std::cout << std::left << std::setw(22) << bc.label
              << "  size=" << std::setw(9) << bc.size
              << "  mean=" << std::fixed << std::setprecision(2)
              << std::setw(8) << stats.mean_ms   << " ms"
              << "  thr=" << std::setw(8) << stats.throughput_mbs << " MB/s"
              << "  ±" << stats.ci95_ms << " ms (95%CI)"
              << "\n";
}

int main(int argc, char* argv[]) {
    bool large = false;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--large") large = true;

    std::cout << "=== Lab 4 Hash Benchmark ===\n";
    std::cout << std::string(80, '-') << "\n";

    std::vector<BenchCase> cases;
    for (auto [label, algo] : std::vector<std::pair<std::string, hash4::Algo>>{
        {"SHA-256",   hash4::Algo::SHA256},
        {"SHA-512",   hash4::Algo::SHA512},
        {"SHA3-256",  hash4::Algo::SHA3_256},
        {"SHA3-512",  hash4::Algo::SHA3_512},
    }) {
        cases.push_back({label + " 1MiB",   algo, 1ULL << 20});
        cases.push_back({label + " 100MiB", algo, 100ULL * (1 << 20)});
        if (large)
            cases.push_back({label + " 1GiB", algo, 1ULL << 30});
    }

    for (auto& bc : cases) run(bc);

    std::cout << std::string(80, '-') << "\n"
              << "Note: SHA-3 uses Keccak sponge (no hardware SHA3 on most CPUs) → typically\n"
              << "      slower than SHA-2 (Merkle-Damgard with hardware AES-SHA extensions).\n"
              << "Re-run with --large to include 1 GiB test (takes several minutes).\n";
    return 0;
}
