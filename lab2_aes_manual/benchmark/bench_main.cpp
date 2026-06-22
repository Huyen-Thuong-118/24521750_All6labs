// bench_main.cpp — AES-128-CTR throughput benchmark
// Follows performance methodology from 00_master_overview.md §3.7:
//   warm-up 1-2s → N=30 independent runs → mean/median/stddev/95% CI
// Output: human-readable table + CSV (stdout)
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <numeric>
#include "aes_core.hpp"
#include "ctr_mode.hpp"

using Clock = std::chrono::steady_clock;
using ns    = std::chrono::nanoseconds;

#ifdef _WIN32
static const char* OS_NAME = "Windows";
#else
static const char* OS_NAME = "Linux";
#endif

struct Stats {
    double mean_ms, median_ms, stddev_ms, ci95_ms, throughput_mbs;
};

static Stats compute_stats(std::vector<double>& times_ms, size_t data_bytes) {
    std::sort(times_ms.begin(), times_ms.end());
    int n = (int)times_ms.size();
    double sum = std::accumulate(times_ms.begin(), times_ms.end(), 0.0);
    double mean = sum / n;

    double med = (n % 2 == 0)
        ? (times_ms[n/2 - 1] + times_ms[n/2]) / 2.0
        : times_ms[n/2];

    double var = 0;
    for (double t : times_ms) var += (t - mean) * (t - mean);
    double sd = std::sqrt(var / n);
    double ci95 = 1.960 * sd / std::sqrt((double)n);

    double mb_per_op = (double)data_bytes / (1024.0 * 1024.0);
    double throughput = mb_per_op / (mean / 1000.0); // MB/s

    return {mean, med, sd, ci95, throughput};
}

// ── Key expansion latency ─────────────────────────────────────────────────────
static Stats bench_key_expansion(int N) {
    uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                       0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};

    // Warm-up ~1 second
    auto t_warm = Clock::now();
    while (std::chrono::duration_cast<ns>(Clock::now() - t_warm).count() < 1'000'000'000LL) {
        key[0] ^= 0x01;
        aes2::AesCore aes(key);
        (void)aes;
    }
    key[0] = 0x2b; // restore

    std::vector<double> times_ms(N);
    for (int i = 0; i < N; ++i) {
        // Each "operation" = 1000 key expansions
        const int OPS = 1000;
        key[0] ^= (uint8_t)i;
        auto t0 = Clock::now();
        for (int j = 0; j < OPS; ++j) {
            key[1] ^= (uint8_t)j;
            aes2::AesCore aes(key);
            (void)aes;
        }
        auto t1 = Clock::now();
        times_ms[i] = (double)std::chrono::duration_cast<ns>(t1-t0).count() / (1e6 * OPS);
    }
    return compute_stats(times_ms, 16); // 16B per key expansion
}

// ── Single block encrypt latency ──────────────────────────────────────────────
static Stats bench_encrypt_block(int N) {
    uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                       0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    uint8_t pt[16]  = {0x32,0x43,0xf6,0xa8,0x88,0x5a,0x30,0x8d,
                       0x31,0x31,0x98,0xa2,0xe0,0x37,0x07,0x34};
    uint8_t ct[16]  = {};
    aes2::AesCore aes(key);

    // Warm-up
    auto t_warm = Clock::now();
    while (std::chrono::duration_cast<ns>(Clock::now() - t_warm).count() < 1'000'000'000LL) {
        aes.encryptBlock(pt, ct);
        pt[0] ^= ct[0];
    }

    const int OPS = 1000;
    std::vector<double> times_ms(N);
    for (int i = 0; i < N; ++i) {
        auto t0 = Clock::now();
        for (int j = 0; j < OPS; ++j) {
            aes.encryptBlock(pt, ct);
            pt[0] ^= ct[0];
        }
        auto t1 = Clock::now();
        times_ms[i] = (double)std::chrono::duration_cast<ns>(t1-t0).count() / (1e6 * OPS);
    }
    return compute_stats(times_ms, 16);
}

// ── CTR throughput at a given data size ──────────────────────────────────────
static Stats bench_ctr(size_t data_bytes, int N) {
    uint8_t key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
                       0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
    uint8_t iv[16]  = {0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,
                       0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff};

    std::vector<uint8_t> pt(data_bytes, 0xAB);
    std::vector<uint8_t> ct(data_bytes);

    aes2::AesCore aes(key);
    aes2::CtrMode ctr(aes);

    // Warm-up ~1 second
    auto t_warm = Clock::now();
    while (std::chrono::duration_cast<ns>(Clock::now() - t_warm).count() < 1'000'000'000LL)
        ctr.process(pt.data(), ct.data(), data_bytes, iv);

    std::vector<double> times_ms(N);
    for (int i = 0; i < N; ++i) {
        auto t0 = Clock::now();
        ctr.process(pt.data(), ct.data(), data_bytes, iv);
        auto t1 = Clock::now();
        times_ms[i] = (double)std::chrono::duration_cast<ns>(t1-t0).count() / 1e6;
    }
    return compute_stats(times_ms, data_bytes);
}

int main(int argc, char** argv) {
    int N = 30;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--rounds") N = std::atoi(argv[i+1]);
    if (N < 2) N = 2;

    printf("=== AES-128-CTR Benchmark (manual, table-based S-box) ===\n");
    printf("OS: %s | N=%d independent runs per case\n\n", OS_NAME, N);

    // ── Key expansion ──────────────────────────────────────────────────────────
    printf("KeyExpansion (1000 ops/sample):\n");
    auto ke = bench_key_expansion(N);
    printf("  mean=%.3f ms/op  median=%.3f  stddev=%.4f  CI95=±%.4f\n\n",
           ke.mean_ms, ke.median_ms, ke.stddev_ms, ke.ci95_ms);

    // ── Single block ───────────────────────────────────────────────────────────
    printf("encryptBlock (1000 ops/sample):\n");
    auto blk = bench_encrypt_block(N);
    printf("  mean=%.4f ms/op  median=%.4f  stddev=%.5f  CI95=±%.5f\n\n",
           blk.mean_ms, blk.median_ms, blk.stddev_ms, blk.ci95_ms);

    // ── CTR throughput ─────────────────────────────────────────────────────────
    struct SizeEntry { size_t bytes; const char* label; };
    const SizeEntry SIZES[] = {
        {1ULL*1024,               "1 KiB"},
        {16ULL*1024,              "16 KiB"},
        {256ULL*1024,             "256 KiB"},
        {1ULL*1024*1024,          "1 MiB"},
        {10ULL*1024*1024,         "10 MiB"},
        {100ULL*1024*1024,        "100 MiB"},
        {1ULL*1024*1024*1024,     "1 GiB"},
    };

    printf("CTR-AES128 throughput (N=%d per size):\n", N);
    printf("%-10s %10s %10s %10s %10s %12s\n",
           "Size", "Mean ms", "Median ms", "StdDev ms", "CI95 ±ms", "Throughput");
    printf("%-10s %10s %10s %10s %10s %12s\n",
           "----------", "--------", "---------", "---------", "--------", "----------");

    std::vector<Stats> results;
    std::vector<const char*> labels;

    for (const auto& e : SIZES) {
        // Skip 1 GiB if not enough RAM (>= 2 GiB needed for pt+ct)
        if (e.bytes >= 1ULL*1024*1024*1024) {
            // Attempt; on failure (allocation), skip gracefully
            try {
                auto s = bench_ctr(e.bytes, N);
                printf("%-10s %10.2f %10.2f %10.3f %10.4f %10.1f MB/s\n",
                       e.label, s.mean_ms, s.median_ms, s.stddev_ms, s.ci95_ms, s.throughput_mbs);
                results.push_back(s);
                labels.push_back(e.label);
            } catch (...) {
                printf("%-10s  (skipped — insufficient memory)\n", e.label);
            }
        } else {
            auto s = bench_ctr(e.bytes, N);
            printf("%-10s %10.2f %10.2f %10.3f %10.4f %10.1f MB/s\n",
                   e.label, s.mean_ms, s.median_ms, s.stddev_ms, s.ci95_ms, s.throughput_mbs);
            results.push_back(s);
            labels.push_back(e.label);
        }
    }

    // ── CSV output ─────────────────────────────────────────────────────────────
    printf("\nCSV:\n");
    printf("algo,mode,os,size_label,size_bytes,"
           "latency_ms_mean,latency_ms_median,latency_ms_stddev,ci95_ms,throughput_mb_s,N\n");
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& s = results[i];
        // map label back to bytes
        size_t bytes = 0;
        for (const auto& e : SIZES) if (std::string(e.label) == labels[i]) bytes = e.bytes;
        printf("AES-128,CTR,%s,%s,%zu,%.4f,%.4f,%.5f,%.5f,%.2f,%d\n",
               OS_NAME, labels[i], bytes,
               s.mean_ms, s.median_ms, s.stddev_ms, s.ci95_ms, s.throughput_mbs, N);
    }

    return 0;
}
