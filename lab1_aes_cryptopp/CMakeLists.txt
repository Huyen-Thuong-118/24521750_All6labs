find_package(cryptopp CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

# ── Static library (shared by tool + tests) ───────────────────────────────────
add_library(lab1_lib STATIC
    src/aes_service.cpp
    src/key_manager.cpp
    src/nonce_manager.cpp
    src/sidecar.cpp
)
target_include_directories(lab1_lib PUBLIC include)
target_link_libraries(lab1_lib PUBLIC common cryptopp::cryptopp nlohmann_json::nlohmann_json)

# ── aestool executable ────────────────────────────────────────────────────────
add_executable(lab1_aestool src/main.cpp)
set_target_properties(lab1_aestool PROPERTIES OUTPUT_NAME "aestool")
target_link_libraries(lab1_aestool PRIVATE lab1_lib)

# ── Tests ─────────────────────────────────────────────────────────────────────
add_executable(lab1_tests
    tests/test_modes.cpp
    tests/test_aead.cpp
    tests/test_negative.cpp
)
target_link_libraries(lab1_tests PRIVATE lab1_lib Catch2::Catch2WithMain)
add_test(NAME lab1_tests COMMAND lab1_tests)
