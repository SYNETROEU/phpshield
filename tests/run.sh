#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

EXT="${PHPSHIELD_EXT:-build/modules/phpshield.so}"
PHP_BIN="${PHP_BIN:-php}"

if [ ! -f "$EXT" ]; then
  echo "missing extension: $EXT" >&2
  exit 1
fi

"$PHP_BIN" -d "extension=$EXT" -m | grep -q phpshield

bin/phpshield init
bin/phpshield encode tests/fixtures/simple tmp/simple --key-file tmp/master.key

test "$(cat tmp/simple/index.php)" = "<?php
return phpshield_load(__FILE__);"

if command -v strings >/dev/null 2>&1; then
  ! strings tmp/simple/app.pshield | grep -q "secret fixture phrase"
fi

key="$(cat tmp/master.key)"
"$PHP_BIN" -d "extension=$EXT" \
  -d phpshield.bundle=tmp/simple/app.pshield \
  -d "phpshield.key=$key" \
  tmp/simple/index.php | grep -q "fixture ok"

if "$PHP_BIN" -d "extension=$EXT" \
  -d phpshield.bundle=tmp/simple/app.pshield \
  -d "phpshield.key=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA" \
  tmp/simple/index.php >"$TMPDIR/phpshield-wrong-key.out" 2>&1; then
  echo "wrong key unexpectedly passed" >&2
  exit 1
fi

cp tmp/simple/app.pshield tmp/simple/app.manifest-tampered.pshield
printf X | dd of=tmp/simple/app.manifest-tampered.pshield bs=1 seek=60 count=1 conv=notrunc >/dev/null 2>&1
if "$PHP_BIN" -d "extension=$EXT" \
  -d phpshield.bundle=tmp/simple/app.manifest-tampered.pshield \
  -d "phpshield.key=$key" \
  tmp/simple/index.php >"$TMPDIR/phpshield-manifest.out" 2>&1; then
  echo "manifest tamper unexpectedly passed" >&2
  exit 1
fi

cp tmp/simple/app.pshield tmp/simple/app.payload-tampered.pshield
size="$(wc -c < tmp/simple/app.payload-tampered.pshield | tr -d ' ')"
seek=$((size - 4))
printf Z | dd of=tmp/simple/app.payload-tampered.pshield bs=1 seek="$seek" count=1 conv=notrunc >/dev/null 2>&1
if "$PHP_BIN" -d "extension=$EXT" \
  -d phpshield.bundle=tmp/simple/app.payload-tampered.pshield \
  -d "phpshield.key=$key" \
  tmp/simple/index.php >"$TMPDIR/phpshield-payload.out" 2>&1; then
  echo "payload tamper unexpectedly passed" >&2
  exit 1
fi

bin/phpshield encode tests/fixtures/simple tmp/licensed --key-file tmp/master.key --require-license --product-id=demo
if "$PHP_BIN" -d "extension=$EXT" \
  -d phpshield.bundle=tmp/licensed/app.pshield \
  -d "phpshield.key=$key" \
  tmp/licensed/index.php >"$TMPDIR/phpshield-missing-license.out" 2>&1; then
  echo "missing license unexpectedly passed" >&2
  exit 1
fi

bin/phpshield make-license --private-key tmp/master.key --out tmp/license.pslic --product-id=demo
"$PHP_BIN" -d "extension=$EXT" \
  -d phpshield.bundle=tmp/licensed/app.pshield \
  -d phpshield.license=tmp/license.pslic \
  -d "phpshield.key=$key" \
  tmp/licensed/index.php | grep -q "fixture ok"

bin/phpshield make-license --private-key tmp/master.key --out tmp/expired.pslic --product-id=demo --expires-at=2000-01-01T00:00:00Z
if "$PHP_BIN" -d "extension=$EXT" \
  -d phpshield.bundle=tmp/licensed/app.pshield \
  -d phpshield.license=tmp/expired.pslic \
  -d "phpshield.key=$key" \
  tmp/licensed/index.php >"$TMPDIR/phpshield-expired.out" 2>&1; then
  echo "expired license unexpectedly passed" >&2
  exit 1
fi

if "$PHP_BIN" -r 'exit(function_exists("sodium_crypto_sign_keypair") ? 0 : 1);'; then
  "$PHP_BIN" -r '$p=sodium_crypto_sign_keypair(); file_put_contents("tmp/license.ed25519.sk", sodium_crypto_sign_secretkey($p)); file_put_contents("tmp/license.ed25519.pub", sodium_crypto_sign_publickey($p));'
  bin/phpshield encode tests/fixtures/simple tmp/ed25519 --key-file tmp/master.key --require-license --product-id=demo --license-public-key=tmp/license.ed25519.pub
  bin/phpshield make-license --private-key tmp/license.ed25519.sk --out tmp/ed25519.pslic --product-id=demo
  "$PHP_BIN" -d "extension=$EXT" \
    -d phpshield.bundle=tmp/ed25519/app.pshield \
    -d phpshield.license=tmp/ed25519.pslic \
    -d "phpshield.key=$key" \
    tmp/ed25519/index.php | grep -q "fixture ok"
  if "$PHP_BIN" -d "extension=$EXT" \
    -d phpshield.bundle=tmp/ed25519/app.pshield \
    -d phpshield.license=tmp/license.pslic \
    -d "phpshield.key=$key" \
    tmp/ed25519/index.php >"$TMPDIR/phpshield-hmac-against-ed.out" 2>&1; then
    echo "development HMAC license unexpectedly passed for Ed25519 bundle" >&2
    exit 1
  fi
fi

if command -v phpize >/dev/null 2>&1; then
  make -n phpize-build >/dev/null
fi

echo "phpshield tests ok"
