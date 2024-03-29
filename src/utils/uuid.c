/*
 * Universally Unique IDentifier (UUID)
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
#include "crypto.h"
#include "sha1.h"
#include "uuid.h"

int uuid_str2bin(const char *str, u8 *bin)
{
        const char *pos;
        u8 *opos;

        pos = str;
        opos = bin;

        if (hexstr2bin(pos, opos, 4))
                return -1;
        pos += 8;
        opos += 4;

        if (*pos++ != '-' || hexstr2bin(pos, opos, 2))
                return -1;
        pos += 4;
        opos += 2;

        if (*pos++ != '-' || hexstr2bin(pos, opos, 2))
                return -1;
        pos += 4;
        opos += 2;

        if (*pos++ != '-' || hexstr2bin(pos, opos, 2))
                return -1;
        pos += 4;
        opos += 2;

        if (*pos++ != '-' || hexstr2bin(pos, opos, 6))
                return -1;

        return 0;
}


int uuid_bin2str(const u8 *bin, char *str, size_t max_len)
{
        int len;
        len = os_snprintf(str, max_len, "%02x%02x%02x%02x-%02x%02x-%02x%02x-"
                          "%02x%02x-%02x%02x%02x%02x%02x%02x",
                          bin[0], bin[1], bin[2], bin[3],
                          bin[4], bin[5], bin[6], bin[7],
                          bin[8], bin[9], bin[10], bin[11],
                          bin[12], bin[13], bin[14], bin[15]);
        if (len < 0 || (size_t) len >= max_len)
                return -1;
        return 0;
}


int is_nil_uuid(const u8 *uuid)
{
        int i;
        for (i = 0; i < UUID_LEN; i++)
                if (uuid[i])
                        return 0;
        return 1;
}


void uuid_gen_mac_addr(const u8 *mac_addr, u8 *uuid)
{
        const u8 *addr[2];
        size_t len[2];
        u8 hash[SHA1_MAC_LEN];
        u8 nsid[16] = {
                0x52, 0x64, 0x80, 0xf8,
                0xc9, 0x9b,
                0x4b, 0xe5,
                0xa6, 0x55,
                0x58, 0xed, 0x5f, 0x5d, 0x60, 0x84
        };

        addr[0] = nsid;
        len[0] = sizeof(nsid);
        addr[1] = mac_addr;
        len[1] = 6;
        sha1_vector(2, addr, len, hash);
        os_memcpy(uuid, hash, 16);

        /* Version: 5 = named-based version using SHA-1 */
        uuid[6] = (5 << 4) | (uuid[6] & 0x0f);

        /* Variant specified in RFC 4122 */
        uuid[8] = 0x80 | (uuid[8] & 0x3f);
}
