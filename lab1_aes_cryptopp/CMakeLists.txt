# lab1_aes_cryptopp — AES with Crypto++ (8 modes)
# Constraint: Crypto++ only (no OpenSSL, no libsodium)

find_package(cryptopp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# ── Main CLI tool ─────────────────────────────────────────────────────────────
add_executable(aestool
    src/main.cpp
    src/aes_modes.cpp
)
target_include_directories(aestool PRIVATE src)
target_link_libraries(aestool PRIVATE
    cryptopp::cryptopp
    nlohmann_json::nlohmann_json
)
set_target_properties(aestool PROPERTIES OUTPUT_NAME "aestool")

# ── Standalone benchmark (outputs CSV) ───────────────────────────────────────
add_executable(aes_bench
    benchmark/bench_main.cpp
    src/aes_modes.cpp
)
target_include_directories(aes_bench PRIVATE src)
target_link_libraries(aes_bench PRIVATE
    cryptopp::cryptopp
)
set_target_properties(aes_bench PROPERTIES OUTPUT_NAME "aes_bench")

# ── Unit tests (Catch2) ───────────────────────────────────────────────────────
add_executable(test_lab1
    tests/test_modes.cpp
    tests/test_aead.cpp
    tests/test_negative.cpp
    tests/test_kat.cpp
    src/aes_modes.cpp
)
target_include_directories(test_lab1 PRIVATE src)
target_link_libraries(test_lab1 PRIVATE
    cryptopp::cryptopp
    nlohmann_json::nlohmann_json
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(test_lab1)
