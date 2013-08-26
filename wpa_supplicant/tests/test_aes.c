/*
 * Test program for AES
 * Copyright (c) 2003-2006, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto.h"
#include "aes_wrap.h"

#define BLOCK_SIZE 16

static void test_aes_perf(void)
{
#if 0 /* this did not seem to work with new compiler?! */
#ifdef __i386__
#define rdtscll(val) \
     __asm__ __volatile__("rdtsc" : "=A" (val))
        const int num_iters = 10;
        int i;
        unsigned int start, end;
        u8 key[16], pt[16], ct[16];
        void *ctx;

        printf("keySetupEnc:");
        for (i = 0; i < num_iters; i++) {
                rdtscll(start);
                ctx = aes_encrypt_init(key, 16);
                rdtscll(end);
                aes_encrypt_deinit(ctx);
                printf(" %d", end - start);
        }
        printf("\n");

        printf("Encrypt:");
        ctx = aes_encrypt_init(key, 16);
        for (i = 0; i < num_iters; i++) {
                rdtscll(start);
                aes_encrypt(ctx, pt, ct);
                rdtscll(end);
                printf(" %d", end - start);
        }
        aes_encrypt_deinit(ctx);
        printf("\n");
#endif /* __i386__ */
#endif
}


static int test_eax(void)
{
        u8 msg[] = { 0xF7, 0xFB };
        u8 key[] = { 0x91, 0x94, 0x5D, 0x3F, 0x4D, 0xCB, 0xEE, 0x0B,
                     0xF4, 0x5E, 0xF5, 0x22, 0x55, 0xF0, 0x95, 0xA4 };
        u8 nonce[] = { 0xBE, 0xCA, 0xF0, 0x43, 0xB0, 0xA2, 0x3D, 0x84,
                       0x31, 0x94, 0xBA, 0x97, 0x2C, 0x66, 0xDE, 0xBD };
        u8 hdr[] = { 0xFA, 0x3B, 0xFD, 0x48, 0x06, 0xEB, 0x53, 0xFA };
        u8 cipher[] = { 0x19, 0xDD, 0x5C, 0x4C, 0x93, 0x31, 0x04, 0x9D,
                        0x0B, 0xDA, 0xB0, 0x27, 0x74, 0x08, 0xF6, 0x79,
                        0x67, 0xE5 };
        u8 data[sizeof(msg)], tag[BLOCK_SIZE];

        memcpy(data, msg, sizeof(msg));
        if (aes_128_eax_encrypt(key, nonce, sizeof(nonce), hdr, sizeof(hdr),
                                data, sizeof(data), tag)) {
                printf("AES-128 EAX mode encryption failed\n");
                return 1;
        }
        if (memcmp(data, cipher, sizeof(data)) != 0) {
                printf("AES-128 EAX mode encryption returned invalid cipher "
                       "text\n");
                return 1;
        }
        if (memcmp(tag, cipher + sizeof(data), BLOCK_SIZE) != 0) {
                printf("AES-128 EAX mode encryption returned invalid tag\n");
                return 1;
        }

        if (aes_128_eax_decrypt(key, nonce, sizeof(nonce), hdr, sizeof(hdr),
                                data, sizeof(data), tag)) {
                printf("AES-128 EAX mode decryption failed\n");
                return 1;
        }
        if (memcmp(data, msg, sizeof(data)) != 0) {
                printf("AES-128 EAX mode decryption returned invalid plain "
                       "text\n");
                return 1;
        }

        return 0;
}


static int test_cbc(void)
{
        struct cbc_test_vector {
                u8 key[16];
                u8 iv[16];
                u8 plain[32];
                u8 cipher[32];
                size_t len;
        } vectors[] = {
                {
                        { 0x06, 0xa9, 0x21, 0x40, 0x36, 0xb8, 0xa1, 0x5b,
                          0x51, 0x2e, 0x03, 0xd5, 0x34, 0x12, 0x00, 0x06 },
                        { 0x3d, 0xaf, 0xba, 0x42, 0x9d, 0x9e, 0xb4, 0x30,
                          0xb4, 0x22, 0xda, 0x80, 0x2c, 0x9f, 0xac, 0x41 },
                        "Single block msg",
                        { 0xe3, 0x53, 0x77, 0x9c, 0x10, 0x79, 0xae, 0xb8,
                          0x27, 0x08, 0x94, 0x2d, 0xbe, 0x77, 0x18, 0x1a },
                        16
                },
                {
                        { 0xc2, 0x86, 0x69, 0x6d, 0x88, 0x7c, 0x9a, 0xa0,
                          0x61, 0x1b, 0xbb, 0x3e, 0x20, 0x25, 0xa4, 0x5a },
                        { 0x56, 0x2e, 0x17, 0x99, 0x6d, 0x09, 0x3d, 0x28,
                          0xdd, 0xb3, 0xba, 0x69, 0x5a, 0x2e, 0x6f, 0x58 },
                        { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                          0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                          0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                          0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f },
                        { 0xd2, 0x96, 0xcd, 0x94, 0xc2, 0xcc, 0xcf, 0x8a,
                          0x3a, 0x86, 0x30, 0x28, 0xb5, 0xe1, 0xdc, 0x0a,
                          0x75, 0x86, 0x60, 0x2d, 0x25, 0x3c, 0xff, 0xf9,
                          0x1b, 0x82, 0x66, 0xbe, 0xa6, 0xd6, 0x1a, 0xb1 },
                        32
                }
        };
        int ret = 0;
        u8 *buf;
        unsigned int i;

        for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
                struct cbc_test_vector *tv = &vectors[i];
                buf = malloc(tv->len);
                if (buf == NULL) {
                        ret++;
                        break;
                }
                memcpy(buf, tv->plain, tv->len);
                aes_128_cbc_encrypt(tv->key, tv->iv, buf, tv->len);
                if (memcmp(buf, tv->cipher, tv->len) != 0) {
                        printf("AES-CBC encrypt %d failed\n", i);
                        ret++;
                }
                memcpy(buf, tv->cipher, tv->len);
                aes_128_cbc_decrypt(tv->key, tv->iv, buf, tv->len);
                if (memcmp(buf, tv->plain, tv->len) != 0) {
                        printf("AES-CBC decrypt %d failed\n", i);
                        ret++;
                }
                free(buf);
        }

        return ret;
}


/* OMAC1 AES-128 test vectors from
 * http://csrc.nist.gov/CryptoToolkit/modes/proposedmodes/omac/omac-ad.pdf
 * which are same as the examples from NIST SP800-38B
 * http://csrc.nist.gov/CryptoToolkit/modes/800-38_Series_Publications/SP800-38B.pdf
 */

struct omac1_test_vector {
        u8 k[16];
        u8 msg[64];
        int msg_len;
        u8 tag[16];
};

static struct omac1_test_vector test_vectors[] =
{
        {
                { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
                { },
                0,
                { 0xbb, 0x1d, 0x69, 0x29, 0xe9, 0x59, 0x37, 0x28,
                  0x7f, 0xa3, 0x7d, 0x12, 0x9b, 0x75, 0x67, 0x46 }
        },
        {
                { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
                { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a},
                16,
                { 0x07, 0x0a, 0x16, 0xb4, 0x6b, 0x4d, 0x41, 0x44,
                  0xf7, 0x9b, 0xdd, 0x9d, 0xd0, 0x4a, 0x28, 0x7c }
        },
        {
                { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
                { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
                  0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
                  0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
                  0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11 },
                40,
                { 0xdf, 0xa6, 0x67, 0x47, 0xde, 0x9a, 0xe6, 0x30,
                  0x30, 0xca, 0x32, 0x61, 0x14, 0x97, 0xc8, 0x27 }
        },
        {
                { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                  0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c },
                { 0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
                  0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
                  0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c,
                  0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
                  0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11,
                  0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
                  0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17,
                  0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10 },
                64,
                { 0x51, 0xf0, 0xbe, 0xbf, 0x7e, 0x3b, 0x9d, 0x92,
                  0xfc, 0x49, 0x74, 0x17, 0x79, 0x36, 0x3c, 0xfe }
        },
};


int main(int argc, char *argv[])
{
        u8 kek[] = {
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
        };
        u8 plain[] = {
                0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
        };
        u8 crypt[] = {
                0x1F, 0xA6, 0x8B, 0x0A, 0x81, 0x12, 0xB4, 0x47,
                0xAE, 0xF3, 0x4B, 0xD8, 0xFB, 0x5A, 0x7B, 0x82,
                0x9D, 0x3E, 0x86, 0x23, 0x71, 0xD2, 0xCF, 0xE5
        };
        u8 result[24];
        int ret = 0;
        unsigned int i;
        struct omac1_test_vector *tv;

        if (aes_wrap(kek, 2, plain, result)) {
                printf("AES-WRAP-128-128 reported failure\n");
                ret++;
        }
        if (memcmp(result, crypt, 24) != 0) {
                printf("AES-WRAP-128-128 failed\n");
                ret++;
        }
        if (aes_unwrap(kek, 2, crypt, result)) {
                printf("AES-UNWRAP-128-128 reported failure\n");
                ret++;
        }
        if (memcmp(result, plain, 16) != 0) {
                printf("AES-UNWRAP-128-128 failed\n");
                ret++;
                for (i = 0; i < 16; i++)
                        printf(" %02x", result[i]);
                printf("\n");
        }

        test_aes_perf();

        for (i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]); i++) {
                tv = &test_vectors[i];
                omac1_aes_128(tv->k, tv->msg, tv->msg_len, result);
                if (memcmp(result, tv->tag, 16) != 0) {
                        printf("OMAC1-AES-128 test vector %d failed\n", i);
                        ret++;
                }

                if (tv->msg_len > 1) {
                        const u8 *addr[2];
                        size_t len[2];

                        addr[0] = tv->msg;
                        len[0] = 1;
                        addr[1] = tv->msg + 1;
                        len[1] = tv->msg_len - 1;

                        omac1_aes_128_vector(tv->k, 2, addr, len, result);
                        if (memcmp(result, tv->tag, 16) != 0) {
                                printf("OMAC1-AES-128(vector) test vector %d "
                                       "failed\n", i);
                                ret++;
                        }
                }
        }

        ret += test_eax();

        ret += test_cbc();

        if (ret)
                printf("FAILED!\n");

        return ret;
}
