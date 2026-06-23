#include "memcrypt.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#else
#include <sys/random.h>
#endif

void phpshield_memcrypt_encrypt(unsigned char *data, size_t len, unsigned char *key, size_t key_len)
{
    if (!data || !key || key_len == 0) return;
    
    // XOR with rolling key (fast, good enough for memory protection)
    for (size_t i = 0; i < len; i++) {
        data[i] ^= key[i % key_len];
        // Mix in position for better diffusion
        data[i] ^= (unsigned char)(i & 0xFF);
    }
}

void phpshield_memcrypt_decrypt(unsigned char *data, size_t len, unsigned char *key, size_t key_len)
{
    // XOR is symmetric
    phpshield_memcrypt_encrypt(data, len, key, key_len);
}

void phpshield_memcrypt_gen_key(unsigned char *key, size_t len)
{
#ifdef _WIN32
    // Use CryptGenRandom on Windows
    HCRYPTPROV hProv;
    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        CryptGenRandom(hProv, (DWORD)len, key);
        CryptReleaseContext(hProv, 0);
    } else {
        // Fallback to time-based if crypto fails
        for (size_t i = 0; i < len; i++) {
            key[i] = (unsigned char)((GetTickCount() >> (i % 4)) ^ i);
        }
    }
#else
    // Use getrandom on Linux
    if (getrandom(key, len, 0) != (ssize_t)len) {
        // Fallback to /dev/urandom
        FILE *fp = fopen("/dev/urandom", "rb");
        if (fp) {
            fread(key, 1, len, fp);
            fclose(fp);
        }
    }
#endif
}
