/*
 * Hashing utilities - EVP API (OpenSSL 3 compatible)
 * Replaces deprecated SHA256_Init/RIPEMD160_Init with EVP_Digest* API
 */

#include <openssl/evp.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "hashing.h"
#include "sha3/sha3.h"

int sha256(const unsigned char *data, size_t length, unsigned char *digest) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        printf("Failed to create SHA256 context\n");
        return 1;
    }
    unsigned int digest_len;
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, data, length) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        printf("SHA256 computation failed\n");
        EVP_MD_CTX_free(ctx);
        return 1;
    }
    EVP_MD_CTX_free(ctx);
    return 0;
}

int sha256_4(size_t length, const unsigned char *data0, const unsigned char *data1,
             const unsigned char *data2, const unsigned char *data3,
             unsigned char *digest0, unsigned char *digest1,
             unsigned char *digest2, unsigned char *digest3) {
    // Process 4 SHA-256 sequentially using EVP API
    // On ARM64, callers should prefer the NEON batch functions instead
    const unsigned char *data[4] = { data0, data1, data2, data3 };
    unsigned char *digest[4] = { digest0, digest1, digest2, digest3 };

    for (int i = 0; i < 4; i++) {
        if (sha256(data[i], length, digest[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

int keccak(const unsigned char *data, size_t length, unsigned char *digest) {
    SHA3_256_CTX ctx;
    SHA3_256_Init(&ctx);
    SHA3_256_Update(&ctx, data, length);
    KECCAK_256_Final(digest, &ctx);
    return 0;
}

int rmd160(const unsigned char *data, size_t length, unsigned char *digest) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        printf("Failed to create RIPEMD-160 context\n");
        return 1;
    }
    unsigned int digest_len;
    if (EVP_DigestInit_ex(ctx, EVP_ripemd160(), NULL) != 1 ||
        EVP_DigestUpdate(ctx, data, length) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        printf("RIPEMD-160 computation failed\n");
        EVP_MD_CTX_free(ctx);
        return 1;
    }
    EVP_MD_CTX_free(ctx);
    return 0;
}

int rmd160_4(size_t length, const unsigned char *data0, const unsigned char *data1,
                const unsigned char *data2, const unsigned char *data3,
                unsigned char *digest0, unsigned char *digest1,
                unsigned char *digest2, unsigned char *digest3) {
    const unsigned char *data[4] = { data0, data1, data2, data3 };
    unsigned char *digest[4] = { digest0, digest1, digest2, digest3 };

    for (int i = 0; i < 4; i++) {
        if (rmd160(data[i], length, digest[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

bool sha256_file(const char* file_name, uint8_t* digest) {
    FILE* file = fopen(file_name, "rb");
    if (file == NULL) {
        printf("Failed to open file: %s\n", file_name);
        return false;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        printf("Failed to create SHA256 context\n");
        fclose(file);
        return false;
    }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        printf("Failed to initialize SHA256\n");
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return false;
    }

    uint8_t buffer[8192];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(ctx, buffer, bytes_read) != 1) {
            printf("Failed to update digest\n");
            EVP_MD_CTX_free(ctx);
            fclose(file);
            return false;
        }
    }

    unsigned int digest_len;
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        printf("Failed to finalize digest\n");
        EVP_MD_CTX_free(ctx);
        fclose(file);
        return false;
    }

    EVP_MD_CTX_free(ctx);
    fclose(file);
    return true;
}
