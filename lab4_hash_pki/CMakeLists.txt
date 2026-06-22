find_package(OpenSSL REQUIRED)
find_package(cryptopp CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ─── hashtool (main CLI) ─────────────────────────────────────────────────────
add_executable(hashtool
    src/main.cpp
    src/hasher.cpp
    src/cert_parser.cpp
    src/sha256_ext.cpp
)
target_include_directories(hashtool PRIVATE include)
target_link_libraries(hashtool PRIVATE common OpenSSL::Crypto cryptopp::cryptopp
                                       nlohmann_json::nlohmann_json)

# ─── lab4_bench ──────────────────────────────────────────────────────────────
add_executable(lab4_bench benchmark/bench_main.cpp src/hasher.cpp)
target_include_directories(lab4_bench PRIVATE include)
target_link_libraries(lab4_bench PRIVATE common OpenSSL::Crypto)

# ─── lab4_tests ──────────────────────────────────────────────────────────────
add_executable(lab4_tests
    tests/test_hash_kat.cpp
    tests/test_x509.cpp
    tests/test_negative.cpp
    tests/test_length_ext.cpp
    src/hasher.cpp
    src/cert_parser.cpp
    src/sha256_ext.cpp
)
target_include_directories(lab4_tests PRIVATE include)
target_compile_definitions(lab4_tests PRIVATE
    LAB4_KAT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/kat"
)
target_link_libraries(lab4_tests PRIVATE
    common
    OpenSSL::Crypto
    cryptopp::cryptopp
    nlohmann_json::nlohmann_json
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(lab4_tests)
