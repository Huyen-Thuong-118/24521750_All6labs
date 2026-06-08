find_package(cryptopp CONFIG REQUIRED)

add_library(common STATIC
    src/cli_parser.cpp
    src/codec.cpp
    src/file_io.cpp
    src/kat_runner.cpp
    src/benchmark.cpp
    src/rng.cpp
)
target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(common PUBLIC cryptopp::cryptopp)

find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    target_link_libraries(common PUBLIC nlohmann_json::nlohmann_json)
endif()
