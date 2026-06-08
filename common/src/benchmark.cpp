#include "common/benchmark.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <vector>

namespace common {

BenchStats measure(const std::function<void()>& fn,
                   std::size_t payload_bytes,
                   int n_samples,
                   double warmup_sec)
{
    using clock  = std::chrono::high_resolution_clock;
    using ms_dur = std::chrono::duration<double, std::milli>;

    // Warmup: run continuously for warmup_sec seconds
    auto t_warmup_end = clock::now() +
        std::chrono::duration_cast<clock::duration>(
            std::chrono::duration<double>(warmup_sec));
    while (clock::now() < t_warmup_end) fn();

    // Measure N samples
    std::vector<double> samples(n_samples);
    for (int i = 0; i < n_samples; ++i) {
        auto t0 = clock::now();
        fn();
        samples[i] = ms_dur(clock::now() - t0).count();
    }

    std::sort(samples.begin(), samples.end());

    double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    double mean   = sum / n_samples;
    double median = samples[n_samples / 2];

    double sq_sum = 0.0;
    for (auto s : samples) sq_sum += (s - mean) * (s - mean);
    double stddev = std::sqrt(sq_sum / n_samples);
    double ci95   = 1.96 * stddev / std::sqrt(static_cast<double>(n_samples));

    double throughput = (mean > 0.0)
        ? (static_cast<double>(payload_bytes) / (1024.0 * 1024.0)) / (mean / 1000.0)
        : 0.0;

    return {mean, median, stddev, ci95, throughput};
}

} // namespace common
