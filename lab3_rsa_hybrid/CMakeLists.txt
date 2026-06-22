find_package(cryptopp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)

# ── Core library ──────────────────────────────────────────────────────────────
add_library(lab3_lib STATIC
    src/rsa_oaep.cpp
    src/hybrid.cpp
)
target_include_directories(lab3_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(lab3_lib PUBLIC
    common
    cryptopp::cryptopp
    nlohmann_json::nlohmann_json
)

# ── CLI tool ──────────────────────────────────────────────────────────────────
add_executable(rsatool src/main.cpp)
target_include_directories(rsatool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(rsatool PRIVATE lab3_lib)
set_target_properties(rsatool PROPERTIES OUTPUT_NAME "rsatool")

# ── Benchmark ─────────────────────────────────────────────────────────────────
add_executable(rsa_bench benchmark/bench_main.cpp)
target_include_directories(rsa_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(rsa_bench PRIVATE lab3_lib)
set_target_properties(rsa_bench PROPERTIES OUTPUT_NAME "rsa_bench")

# ── Tests ─────────────────────────────────────────────────────────────────────
add_executable(test_lab3
    tests/test_rsa_oaep.cpp
    tests/test_hybrid.cpp
    tests/test_negative.cpp
)
target_include_directories(test_lab3 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(test_lab3 PRIVATE lab3_lib Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(test_lab3)
