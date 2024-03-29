/*
 * EAP-FAST common helper functions (RFC 4851)
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
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
#include "sha1.h"
#include "tls.h"
#include "eap_defs.h"
#include "eap_tlv_common.h"
#include "eap_fast_common.h"


void eap_fast_put_tlv_hdr(struct wpabuf *buf, u16 type, u16 len)
{
        struct pac_tlv_hdr hdr;
        hdr.type = host_to_be16(type);
        hdr.len = host_to_be16(len);
        wpabuf_put_data(buf, &hdr, sizeof(hdr));
}


void eap_fast_put_tlv(struct wpabuf *buf, u16 type, const void *data,
                             u16 len)
{
        eap_fast_put_tlv_hdr(buf, type, len);
        wpabuf_put_data(buf, data, len);
}


void eap_fast_put_tlv_buf(struct wpabuf *buf, u16 type,
                                 const struct wpabuf *data)
{
        eap_fast_put_tlv_hdr(buf, type, wpabuf_len(data));
        wpabuf_put_buf(buf, data);
}


struct wpabuf * eap_fast_tlv_eap_payload(struct wpabuf *buf)
{
        struct wpabuf *e;

        if (buf == NULL)
                return NULL;

        /* Encapsulate EAP packet in EAP-Payload TLV */
        wpa_printf(MSG_DEBUG, "EAP-FAST: Add EAP-Payload TLV");
        e = wpabuf_alloc(sizeof(struct pac_tlv_hdr) + wpabuf_len(buf));
        if (e == NULL) {
                wpa_printf(MSG_DEBUG, "EAP-FAST: Failed to allocate memory "
                           "for TLV encapsulation");
                wpabuf_free(buf);
                return NULL;
        }
        eap_fast_put_tlv_buf(e,
                             EAP_TLV_TYPE_MANDATORY | EAP_TLV_EAP_PAYLOAD_TLV,
                             buf);
        wpabuf_free(buf);
        return e;
}


void eap_fast_derive_master_secret(const u8 *pac_key, const u8 *server_random,
                                   const u8 *client_random, u8 *master_secret)
{
#define TLS_RANDOM_LEN 32
#define TLS_MASTER_SECRET_LEN 48
        u8 seed[2 * TLS_RANDOM_LEN];

        wpa_hexdump(MSG_DEBUG, "EAP-FAST: client_random",
                    client_random, TLS_RANDOM_LEN);
        wpa_hexdump(MSG_DEBUG, "EAP-FAST: server_random",
                    server_random, TLS_RANDOM_LEN);

        /*
         * RFC 4851, Section 5.1:
         * master_secret = T-PRF(PAC-Key, "PAC to master secret label hash", 
         *                       server_random + client_random, 48)
         */
        os_memcpy(seed, server_random, TLS_RANDOM_LEN);
        os_memcpy(seed + TLS_RANDOM_LEN, client_random, TLS_RANDOM_LEN);
        sha1_t_prf(pac_key, EAP_FAST_PAC_KEY_LEN,
                   "PAC to master secret label hash",
                   seed, sizeof(seed), master_secret, TLS_MASTER_SECRET_LEN);

        wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: master_secret",
                        master_secret, TLS_MASTER_SECRET_LEN);
}


u8 * eap_fast_derive_key(void *ssl_ctx, struct tls_connection *conn,
                         const char *label, size_t len)
{
        struct tls_keys keys;
        u8 *rnd = NULL, *out;
        int block_size;

        block_size = tls_connection_get_keyblock_size(ssl_ctx, conn);
        if (block_size < 0)
                return NULL;

        out = os_malloc(block_size + len);
        if (out == NULL)
                return NULL;

        if (tls_connection_prf(ssl_ctx, conn, label, 1, out, block_size + len)
            == 0) {
                os_memmove(out, out + block_size, len);
                return out;
        }

        if (tls_connection_get_keys(ssl_ctx, conn, &keys))
                goto fail;

        rnd = os_malloc(keys.client_random_len + keys.server_random_len);
        if (rnd == NULL)
                goto fail;

        os_memcpy(rnd, keys.server_random, keys.server_random_len);
        os_memcpy(rnd + keys.server_random_len, keys.client_random,
                  keys.client_random_len);

        wpa_hexdump_key(MSG_MSGDUMP, "EAP-FAST: master_secret for key "
                        "expansion", keys.master_key, keys.master_key_len);
        if (tls_prf(keys.master_key, keys.master_key_len,
                    label, rnd, keys.client_random_len +
                    keys.server_random_len, out, block_size + len))
                goto fail;
        os_free(rnd);
        os_memmove(out, out + block_size, len);
        return out;

fail:
        os_free(rnd);
        os_free(out);
        return NULL;
}


void eap_fast_derive_eap_msk(const u8 *simck, u8 *msk)
{
        /*
         * RFC 4851, Section 5.4: EAP Master Session Key Generation
         * MSK = T-PRF(S-IMCK[j], "Session Key Generating Function", 64)
         */

        sha1_t_prf(simck, EAP_FAST_SIMCK_LEN,
                   "Session Key Generating Function", (u8 *) "", 0,
                   msk, EAP_FAST_KEY_LEN);
        wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: Derived key (MSK)",
                        msk, EAP_FAST_KEY_LEN);
}


void eap_fast_derive_eap_emsk(const u8 *simck, u8 *emsk)
{
        /*
         * RFC 4851, Section 5.4: EAP Master Session Key Genreration
         * EMSK = T-PRF(S-IMCK[j],
         *        "Extended Session Key Generating Function", 64)
         */

        sha1_t_prf(simck, EAP_FAST_SIMCK_LEN,
                   "Extended Session Key Generating Function", (u8 *) "", 0,
                   emsk, EAP_EMSK_LEN);
        wpa_hexdump_key(MSG_DEBUG, "EAP-FAST: Derived key (EMSK)",
                        emsk, EAP_EMSK_LEN);
}


int eap_fast_parse_tlv(struct eap_fast_tlv_parse *tlv,
                       int tlv_type, u8 *pos, int len)
{
        switch (tlv_type) {
        case EAP_TLV_EAP_PAYLOAD_TLV:
                wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: EAP-Payload TLV",
                            pos, len);
                if (tlv->eap_payload_tlv) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: More than one "
                                   "EAP-Payload TLV in the message");
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                        return -2;
                }
                tlv->eap_payload_tlv = pos;
                tlv->eap_payload_tlv_len = len;
                break;
        case EAP_TLV_RESULT_TLV:
                wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Result TLV", pos, len);
                if (tlv->result) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: More than one "
                                   "Result TLV in the message");
                        tlv->result = EAP_TLV_RESULT_FAILURE;
                        return -2;
                }
                if (len < 2) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: Too short "
                                   "Result TLV");
                        tlv->result = EAP_TLV_RESULT_FAILURE;
                        break;
                }
                tlv->result = WPA_GET_BE16(pos);
                if (tlv->result != EAP_TLV_RESULT_SUCCESS &&
                    tlv->result != EAP_TLV_RESULT_FAILURE) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: Unknown Result %d",
                                   tlv->result);
                        tlv->result = EAP_TLV_RESULT_FAILURE;
                }
                wpa_printf(MSG_DEBUG, "EAP-FAST: Result: %s",
                           tlv->result == EAP_TLV_RESULT_SUCCESS ?
                           "Success" : "Failure");
                break;
        case EAP_TLV_INTERMEDIATE_RESULT_TLV:
                wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Intermediate Result TLV",
                            pos, len);
                if (len < 2) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: Too short "
                                   "Intermediate-Result TLV");
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                        break;
                }
                if (tlv->iresult) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: More than one "
                                   "Intermediate-Result TLV in the message");
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                        return -2;
                }
                tlv->iresult = WPA_GET_BE16(pos);
                if (tlv->iresult != EAP_TLV_RESULT_SUCCESS &&
                    tlv->iresult != EAP_TLV_RESULT_FAILURE) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: Unknown Intermediate "
                                   "Result %d", tlv->iresult);
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                }
                wpa_printf(MSG_DEBUG, "EAP-FAST: Intermediate Result: %s",
                           tlv->iresult == EAP_TLV_RESULT_SUCCESS ?
                           "Success" : "Failure");
                break;
        case EAP_TLV_CRYPTO_BINDING_TLV:
                wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Crypto-Binding TLV",
                            pos, len);
                if (tlv->crypto_binding) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: More than one "
                                   "Crypto-Binding TLV in the message");
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                        return -2;
                }
                tlv->crypto_binding_len = sizeof(struct eap_tlv_hdr) + len;
                if (tlv->crypto_binding_len < sizeof(*tlv->crypto_binding)) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: Too short "
                                   "Crypto-Binding TLV");
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                        return -2;
                }
                tlv->crypto_binding = (struct eap_tlv_crypto_binding_tlv *)
                        (pos - sizeof(struct eap_tlv_hdr));
                break;
        case EAP_TLV_REQUEST_ACTION_TLV:
                wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: Request-Action TLV",
                            pos, len);
                if (tlv->request_action) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: More than one "
                                   "Request-Action TLV in the message");
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                        return -2;
                }
                if (len < 2) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: Too short "
                                   "Request-Action TLV");
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                        break;
                }
                tlv->request_action = WPA_GET_BE16(pos);
                wpa_printf(MSG_DEBUG, "EAP-FAST: Request-Action: %d",
                           tlv->request_action);
                break;
        case EAP_TLV_PAC_TLV:
                wpa_hexdump(MSG_MSGDUMP, "EAP-FAST: PAC TLV", pos, len);
                if (tlv->pac) {
                        wpa_printf(MSG_DEBUG, "EAP-FAST: More than one "
                                   "PAC TLV in the message");
                        tlv->iresult = EAP_TLV_RESULT_FAILURE;
                        return -2;
                }
                tlv->pac = pos;
                tlv->pac_len = len;
                break;
        default:
                /* Unknown TLV */
                return -1;
        }

        return 0;
}
