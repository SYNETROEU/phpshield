set(PHPSHIELD_SOURCES
  phpshield.c
  src/bundle.c
  src/crypto.c
  src/license.c
  src/manifest.c
  src/ir.c
  src/cache.c
  src/execute.c
  src/pathmap.c
  src/anti_tamper.c
  src/opcode.c
  src/memcrypt.c
)

add_library(phpshield MODULE ${PHPSHIELD_SOURCES})
if(WIN32)
  set_target_properties(phpshield PROPERTIES PREFIX "" OUTPUT_NAME "php_phpshield" SUFFIX ".dll" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules")
  target_link_libraries(phpshield PRIVATE ${PHP_LIBRARY} iphlpapi advapi32 ws2_32)
  target_compile_definitions(phpshield PRIVATE PHP_WIN32 ZEND_WIN32 ZEND_WIN32_FORCE_INLINE)
else()
  set_target_properties(phpshield PROPERTIES PREFIX "" OUTPUT_NAME "phpshield" LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules")
endif()
set_property(TARGET phpshield PROPERTY C_STANDARD 99)
set_property(TARGET phpshield PROPERTY C_STANDARD_REQUIRED ON)
target_include_directories(phpshield PRIVATE ${PHP_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_definitions(phpshield PRIVATE ZEND_ENABLE_STATIC_TSRMLS_CACHE=1 COMPILE_DL_PHPSHIELD ZEND_DEBUG=0 PHP_SHIELD_VERSION="${PROJECT_VERSION}")
if(PHPSHIELD_PRODUCTION)
  target_compile_definitions(phpshield PRIVATE PHPSHIELD_PRODUCTION=1)
endif()

if(PHPSHIELD_ENABLE_SODIUM)
  find_path(SODIUM_INCLUDE_DIR sodium.h HINTS "${SODIUM_ROOT}/include" "${SODIUM_ROOT}/src/libsodium/include")
  find_library(SODIUM_LIBRARY NAMES sodium libsodium HINTS
    "${SODIUM_ROOT}/lib"
    "${SODIUM_ROOT}/bin"
    "${SODIUM_ROOT}/bin/x64/Release/v145/dynamic"
    "${SODIUM_ROOT}/bin/x64/Release/v143/dynamic"
    "${SODIUM_ROOT}"
  )
  if(SODIUM_INCLUDE_DIR AND SODIUM_LIBRARY)
    target_include_directories(phpshield PRIVATE ${SODIUM_INCLUDE_DIR})
    target_link_libraries(phpshield PRIVATE ${SODIUM_LIBRARY})
    target_compile_definitions(phpshield PRIVATE PHPSHIELD_HAVE_SODIUM=1)
  else()
    message(WARNING "libsodium headers/library not found; XChaCha20-Poly1305 bundles will not load")
  endif()
endif()

if(PHPSHIELD_ENABLE_OPENSSL)
  find_package(OpenSSL)
  if(OpenSSL_FOUND)
    target_link_libraries(phpshield PRIVATE OpenSSL::Crypto)
    target_compile_definitions(phpshield PRIVATE PHPSHIELD_HAVE_OPENSSL=1)
  else()
    message(WARNING "OpenSSL development headers/library not found; AES-256-GCM fallback will not load")
  endif()
endif()

if(PHPSHIELD_STRICT_HARDENING AND NOT SODIUM_LIBRARY AND NOT OpenSSL_FOUND)
  message(FATAL_ERROR "No production crypto backend found. Install libsodium or OpenSSL development headers, or configure with -DPHPSHIELD_STRICT_HARDENING=OFF for a loader that refuses production mode.")
endif()

phpshield_apply_hardening(phpshield)
