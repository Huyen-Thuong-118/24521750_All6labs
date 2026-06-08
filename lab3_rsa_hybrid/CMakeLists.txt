find_package(cryptopp CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(rsatool src/main.cpp)
target_include_directories(rsatool PRIVATE include)
target_link_libraries(rsatool PRIVATE common cryptopp::cryptopp OpenSSL::Crypto)
