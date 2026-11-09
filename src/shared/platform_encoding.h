#ifndef ZOV_PLATFORM_ENCODING_H
#define ZOV_PLATFORM_ENCODING_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static const char zov_b64_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char zov_b64url_table[65] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int8_t zov_b64_rev_tbl[256];
static int8_t zov_b64url_rev_tbl[256];
static int zov_b64_init_done = 0;

static inline void zov_b64_init_rev_tbls(void) {
    if (zov_b64_init_done) return;
    memset(zov_b64_rev_tbl, -1, sizeof(zov_b64_rev_tbl));
    memset(zov_b64url_rev_tbl, -1, sizeof(zov_b64url_rev_tbl));

    zov_b64_rev_tbl[(uint8_t)'\n'] = -2;
    zov_b64_rev_tbl[(uint8_t)'\r'] = -2;
    zov_b64_rev_tbl[(uint8_t)'\t'] = -2;
    zov_b64_rev_tbl[(uint8_t)' '] = -2;
    zov_b64url_rev_tbl[(uint8_t)'\n'] = -2;
    zov_b64url_rev_tbl[(uint8_t)'\r'] = -2;
    zov_b64url_rev_tbl[(uint8_t)'\t'] = -2;
    zov_b64url_rev_tbl[(uint8_t)' '] = -2;

    zov_b64_rev_tbl[(uint8_t)'='] = -3;
    zov_b64url_rev_tbl[(uint8_t)'='] = -3;

    for (int i = 0; i < 26; i++) {
        zov_b64_rev_tbl['A' + i] = (int8_t)i;
        zov_b64_rev_tbl['a' + i] = (int8_t)(i + 26);
        zov_b64url_rev_tbl['A' + i] = (int8_t)i;
        zov_b64url_rev_tbl['a' + i] = (int8_t)(i + 26);
    }
    for (int i = 0; i < 10; i++) {
        zov_b64_rev_tbl['0' + i] = (int8_t)(i + 52);
        zov_b64url_rev_tbl['0' + i] = (int8_t)(i + 52);
    }
    zov_b64_rev_tbl[(uint8_t)'+'] = 62;
    zov_b64_rev_tbl[(uint8_t)'/'] = 63;
    zov_b64url_rev_tbl[(uint8_t)'-'] = 62;
    zov_b64url_rev_tbl[(uint8_t)'_'] = 63;

    zov_b64_init_done = 1;
}

static inline int zov_base64_encode_stream(FILE *in_fp, FILE *out_fp, int wrap, int url) {
    const char *tbl = url ? zov_b64url_table : zov_b64_table;
    uint8_t in_buf[49152];
    char out_buf[65536 + 4096];
    int col = 0;
    size_t in_len;
    size_t total_written = 0;

    while ((in_len = fread(in_buf, 1, sizeof(in_buf), in_fp)) > 0) {
        size_t i = 0;
        size_t out_pos = 0;
        size_t full_triples = in_len / 3;

        for (size_t t = 0; t < full_triples; t++) {
            uint32_t b0 = in_buf[i++];
            uint32_t b1 = in_buf[i++];
            uint32_t b2 = in_buf[i++];

            uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
            char c0 = tbl[(triple >> 18) & 0x3F];
            char c1 = tbl[(triple >> 12) & 0x3F];
            char c2 = tbl[(triple >> 6) & 0x3F];
            char c3 = tbl[triple & 0x3F];

            char chars[4] = {c0, c1, c2, c3};
            for (int k = 0; k < 4; k++) {
                if (wrap > 0 && col >= wrap) {
                    out_buf[out_pos++] = (char)10;
                    col = 0;
                }
                out_buf[out_pos++] = chars[k];
                col++;
                total_written++;
            }

            if (out_pos >= 65536) {
                if (fwrite(out_buf, 1, out_pos, out_fp) != out_pos) return 1;
                out_pos = 0;
            }
        }

        size_t rem = in_len - i;
        if (rem > 0) {
            uint32_t b0 = in_buf[i++];
            uint32_t b1 = (rem == 2) ? in_buf[i++] : 0;
            uint32_t triple = (b0 << 16) | (b1 << 8);

            char c0 = tbl[(triple >> 18) & 0x3F];
            char c1 = tbl[(triple >> 12) & 0x3F];
            char c2 = (rem == 2) ? tbl[(triple >> 6) & 0x3F] : 61;
            char c3 = 61;

            char chars[4] = {c0, c1, c2, c3};
            for (int k = 0; k < 4; k++) {
                if (wrap > 0 && col >= wrap) {
                    out_buf[out_pos++] = (char)10;
                    col = 0;
                }
                out_buf[out_pos++] = chars[k];
                col++;
                total_written++;
            }
        }

        if (out_pos > 0) {
            if (fwrite(out_buf, 1, out_pos, out_fp) != out_pos) return 1;
        }
    }

    if (total_written > 0 && wrap > 0) {
        fputc(10, out_fp);
    }
    return ferror(in_fp) ? 1 : 0;
}

static inline int zov_base64_decode_stream(FILE *in_fp, FILE *out_fp, int ignore, int url) {
    zov_b64_init_rev_tbls();
    const int8_t *tbl = url ? zov_b64url_rev_tbl : zov_b64_rev_tbl;

    uint8_t in_buf[65536];
    uint8_t out_buf[65536];
    size_t in_len;
    size_t out_pos = 0;

    uint32_t val = 0;
    int quad = 0;
    int pad = 0;
    int ended = 0;

    while ((in_len = fread(in_buf, 1, sizeof(in_buf), in_fp)) > 0) {
        for (size_t i = 0; i < in_len; i++) {
            uint8_t ch = in_buf[i];
            int8_t v = tbl[ch];

            if (v == -2) continue;
            if (v == -3) {
                ended = 1;
                pad++;
                continue;
            }
            if (v < 0) {
                if (ignore) continue;
                if (out_pos > 0) fwrite(out_buf, 1, out_pos, out_fp);
                return 1;
            }
            if (ended) {
                if (ignore) continue;
                if (out_pos > 0) fwrite(out_buf, 1, out_pos, out_fp);
                return 1;
            }

            val = (val << 6) | (uint32_t)v;
            quad++;

            if (quad == 4) {
                out_buf[out_pos++] = (uint8_t)((val >> 16) & 0xFF);
                out_buf[out_pos++] = (uint8_t)((val >> 8) & 0xFF);
                out_buf[out_pos++] = (uint8_t)(val & 0xFF);
                val = 0;
                quad = 0;
                if (out_pos >= 65530) {
                    if (fwrite(out_buf, 1, out_pos, out_fp) != out_pos) return 1;
                    out_pos = 0;
                }
            }
        }
    }

    if (ferror(in_fp)) {
        if (out_pos > 0) fwrite(out_buf, 1, out_pos, out_fp);
        return 1;
    }

    int err = 0;
    if (quad == 2) {
        out_buf[out_pos++] = (uint8_t)((val >> 4) & 0xFF);
        if ((val & 0x0F) != 0) err = 1;
        if (pad != 0 && pad != 2) err = 1;
    } else if (quad == 3) {
        out_buf[out_pos++] = (uint8_t)((val >> 10) & 0xFF);
        out_buf[out_pos++] = (uint8_t)((val >> 2) & 0xFF);
        if ((val & 0x03) != 0) err = 1;
        if (pad != 0 && pad != 1) err = 1;
    } else if (quad != 0 || pad > 0) {
        err = 1;
    }

    if (out_pos > 0) {
        if (fwrite(out_buf, 1, out_pos, out_fp) != out_pos) return 1;
    }
    return err;

    if (out_pos > 0) {
        if (fwrite(out_buf, 1, out_pos, out_fp) != out_pos) return 1;
    }
    return 0;
}

#endif
