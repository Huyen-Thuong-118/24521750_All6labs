find_package(OpenSSL REQUIRED)   # needs 3.5+ for ML-DSA/ML-KEM native
find_package(Catch2 3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ─── Core sources ─────────────────────────────────────────────────────────────
set(PQ_SRC
    src/mldsa.cpp
    src/mlkem.cpp
    src/pq_cert.cpp
    src/pq_dispatch.cpp
)

# ─── pqtool CLI ───────────────────────────────────────────────────────────────
add_executable(pqtool src/main.cpp ${PQ_SRC})
target_include_directories(pqtool PRIVATE include)
target_link_libraries(pqtool PRIVATE common OpenSSL::Crypto OpenSSL::SSL
                                      nlohmann_json::nlohmann_json)

# ─── Tests ────────────────────────────────────────────────────────────────────
add_executable(lab6_tests
    ${PQ_SRC}
    tests/test_mldsa.cpp
    tests/test_mlkem.cpp
    tests/test_cert.cpp
    tests/test_negative.cpp
)
target_include_directories(lab6_tests PRIVATE include)
target_link_libraries(lab6_tests PRIVATE
    common
    OpenSSL::Crypto
    OpenSSL::SSL
    nlohmann_json::nlohmann_json
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(lab6_tests)

# ─── Benchmark ────────────────────────────────────────────────────────────────
add_executable(lab6_bench ${PQ_SRC} benchmark/bench_main.cpp)
target_include_directories(lab6_bench PRIVATE include)
target_link_libraries(lab6_bench PRIVATE common OpenSSL::Crypto OpenSSL::SSL
                                          nlohmann_json::nlohmann_json)
