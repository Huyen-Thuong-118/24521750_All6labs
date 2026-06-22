# Lab 2: AES-128 thuần C++ — KHÔNG dùng thư viện crypto ngoài
# Constraint: chỉ STL; Crypto++ chỉ được phép trong test_crossval

find_package(Catch2 3 CONFIG REQUIRED)
find_package(cryptopp CONFIG REQUIRED)   # dùng CHỈ trong test_crossval

# ── AES core library (no crypto dependency) ───────────────────────────────────
add_library(aes2_core STATIC
    src/gf256.cpp
    src/aes_core.cpp
    src/ctr_mode.cpp
)
target_include_directories(aes2_core PUBLIC include)

# ── Main CLI tool (must NOT link any crypto lib) ──────────────────────────────
add_executable(lab2_aestool
    src/main.cpp
)
target_include_directories(lab2_aestool PRIVATE include)
target_link_libraries(lab2_aestool PRIVATE aes2_core)
set_target_properties(lab2_aestool PROPERTIES OUTPUT_NAME "aestool2")

# ── Standalone benchmark ──────────────────────────────────────────────────────
add_executable(lab2_bench
    benchmark/bench_main.cpp
)
target_include_directories(lab2_bench PRIVATE include)
target_link_libraries(lab2_bench PRIVATE aes2_core)
set_target_properties(lab2_bench PROPERTIES OUTPUT_NAME "aes2_bench")

# ── Unit tests ────────────────────────────────────────────────────────────────
add_executable(test_lab2
    tests/test_fips197.cpp
    tests/test_ctr.cpp
    tests/test_negative.cpp
)
target_include_directories(test_lab2 PRIVATE include)
target_link_libraries(test_lab2 PRIVATE aes2_core Catch2::Catch2WithMain)

# Cross-validation test links Crypto++ (allowed only in test, not in main tool)
add_executable(test_lab2_crossval
    tests/test_crossval.cpp
)
target_include_directories(test_lab2_crossval PRIVATE include)
target_link_libraries(test_lab2_crossval PRIVATE aes2_core Catch2::Catch2WithMain cryptopp::cryptopp)

include(CTest)
include(Catch)
catch_discover_tests(test_lab2)
catch_discover_tests(test_lab2_crossval)
