#pragma once
#include <cstddef>
#include <functional>

namespace common {

struct BenchStats {
    double mean_ms;
    double median_ms;
    double stddev_ms;
    double ci95_ms;        // 1.96 * stddev / sqrt(n)
    double throughput_mbs;
};

// Đo fn() với warm-up và N>=30 mẫu; payload_bytes dùng tính throughput
BenchStats measure(const std::function<void()>& fn,
                   std::size_t payload_bytes,
                   int n_samples = 30,
                   double warmup_sec = 2.0);

} // namespace common
