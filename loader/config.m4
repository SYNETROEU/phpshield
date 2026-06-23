PHP_ARG_ENABLE(phpshield, whether to enable PHPShield,
[  --enable-phpshield       Enable PHPShield loader])

if test "$PHP_PHPSHIELD" != "no"; then
  AC_CHECK_HEADER([sodium.h], [AC_DEFINE([PHPSHIELD_HAVE_SODIUM], [1], [Have libsodium]) PHPSHIELD_SHARED_LIBADD="$PHPSHIELD_SHARED_LIBADD -lsodium"], [])
  AC_CHECK_HEADER([openssl/evp.h], [AC_DEFINE([PHPSHIELD_HAVE_OPENSSL], [1], [Have OpenSSL]) PHPSHIELD_SHARED_LIBADD="$PHPSHIELD_SHARED_LIBADD -lcrypto"], [])
  AC_DEFINE([COMPILE_DL_PHPSHIELD], [1], [Build shared PHPShield module])
  PHP_SUBST(PHPSHIELD_SHARED_LIBADD)
  PHP_NEW_EXTENSION(phpshield, phpshield.c src/bundle.c src/crypto.c src/license.c src/manifest.c src/ir.c src/cache.c src/execute.c src/pathmap.c src/anti_tamper.c, $ext_shared)
fi
