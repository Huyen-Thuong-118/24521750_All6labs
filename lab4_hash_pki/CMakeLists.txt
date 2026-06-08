find_package(OpenSSL REQUIRED)
find_package(cryptopp CONFIG REQUIRED)

add_executable(hashtool src/main.cpp)
target_include_directories(hashtool PRIVATE include)
target_link_libraries(hashtool PRIVATE common OpenSSL::Crypto cryptopp::cryptopp)
