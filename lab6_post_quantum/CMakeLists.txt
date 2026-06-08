find_package(OpenSSL REQUIRED)   # cần 3.5+ cho ML-DSA/ML-KEM native

# Dùng liboqs nếu OpenSSL < 3.5: cmake -DUSE_LIBOQS=ON
option(USE_LIBOQS "Dùng liboqs thay vì OpenSSL cho ML-DSA/ML-KEM" OFF)
if(USE_LIBOQS)
    find_package(liboqs CONFIG REQUIRED)
endif()

add_executable(pqtool src/main.cpp)
target_include_directories(pqtool PRIVATE include)
target_link_libraries(pqtool PRIVATE common OpenSSL::Crypto)
if(USE_LIBOQS)
    target_link_libraries(pqtool PRIVATE OQS::oqs)
    target_compile_definitions(pqtool PRIVATE USE_LIBOQS=1)
endif()
