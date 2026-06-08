find_package(cryptopp CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(sigtool src/main.cpp)
target_include_directories(sigtool PRIVATE include)
target_link_libraries(sigtool PRIVATE common cryptopp::cryptopp OpenSSL::Crypto)
