cmake_minimum_required(VERSION 3.20)
project(All6Labs LANGUAGES CXX)

# C++17 bắt buộc, cross-platform (không dùng GNU extensions)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default Release cho single-config generators (Ninja, Make)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Cảnh báo chặt
if(MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_subdirectory(common)
add_subdirectory(lab1_aes_cryptopp)
add_subdirectory(lab2_aes_manual)
add_subdirectory(lab3_rsa_hybrid)
add_subdirectory(lab4_hash_pki)
add_subdirectory(lab5_signatures)
add_subdirectory(lab6_post_quantum)
