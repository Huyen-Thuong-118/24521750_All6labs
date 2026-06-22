find_package(cryptopp CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# ─── Core sources shared by all targets ──────────────────────────────────────
set(SIG5_SRC
    src/ecdsa_signer.cpp
    src/rsapss_signer.cpp
    src/sig_dispatch.cpp
)

# ─── sigtool CLI ─────────────────────────────────────────────────────────────
add_executable(sigtool src/main.cpp ${SIG5_SRC})
target_include_directories(sigtool PRIVATE include)
target_link_libraries(sigtool PRIVATE common cryptopp::cryptopp OpenSSL::Crypto OpenSSL::SSL)
if(MSVC)
    target_compile_options(sigtool PRIVATE /wd4996)
endif()

# ─── Tests ───────────────────────────────────────────────────────────────────
add_executable(lab5_tests
    ${SIG5_SRC}
    tests/test_ecdsa.cpp
    tests/test_rsapss.cpp
    tests/test_negative.cpp
    tests/test_batch.cpp
)
target_include_directories(lab5_tests PRIVATE include)
target_link_libraries(lab5_tests PRIVATE
    common
    cryptopp::cryptopp
    OpenSSL::Crypto
    OpenSSL::SSL
    Catch2::Catch2WithMain
)
if(MSVC)
    target_compile_options(lab5_tests PRIVATE /wd4996)
endif()

include(CTest)
include(Catch)
catch_discover_tests(lab5_tests)

# ─── Benchmark ───────────────────────────────────────────────────────────────
add_executable(lab5_bench ${SIG5_SRC} benchmark/bench_main.cpp)
target_include_directories(lab5_bench PRIVATE include)
target_link_libraries(lab5_bench PRIVATE common cryptopp::cryptopp OpenSSL::Crypto OpenSSL::SSL)
if(MSVC)
    target_compile_options(lab5_bench PRIVATE /wd4996)
endif()
