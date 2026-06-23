cmake_minimum_required(VERSION 3.20)
project(phpshield VERSION 0.1.0 LANGUAGES C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PHPSHIELD_ENABLE_SODIUM "Enable libsodium authenticated encryption" ON)
option(PHPSHIELD_ENABLE_OPENSSL "Enable OpenSSL AES-256-GCM fallback" ON)
option(PHPSHIELD_STRICT_HARDENING "Fail configure if no production crypto backend is available" ON)
option(PHPSHIELD_BUILD_TESTS "Build native loader tests when added" ON)
option(PHPSHIELD_PRODUCTION "Build loader in production mode; development HMAC licenses are rejected" OFF)
set(SODIUM_ROOT "" CACHE PATH "Root directory containing libsodium include and library directories")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerHardening)
find_package(PHP REQUIRED)

add_subdirectory(loader)
