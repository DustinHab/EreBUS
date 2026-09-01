/*
 * base64.c -- bytes as letters, RFC 4648.
 */
#include <eb/base64.h>

static const char letters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

u32 base64_encode(const u8 *in, u32 len, char *out, bool pad)
{
    u32 at = 0;
    for (u32 i = 0; i < len; i += 3) {
        u32 v = (u32)in[i] << 16;
        if (i + 1 < len) v |= (u32)in[i + 1] << 8;
        if (i + 2 < len) v |= (u32)in[i + 2];
        out[at++] = letters[(v >> 18) & 63];
        out[at++] = letters[(v >> 12) & 63];
        if (i + 1 < len) out[at++] = letters[(v >> 6) & 63];
        else if (pad)    out[at++] = '=';
        if (i + 2 < len) out[at++] = letters[v & 63];
        else if (pad)    out[at++] = '=';
    }
    out[at] = 0;
    return at;
}

static i32 value_of(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

i32 base64_decode(const char *in, u32 len, u8 *out, u32 max)
{
    u32 acc = 0, bits = 0, n = 0;
    for (u32 i = 0; i < len; i++) {
        char c = in[i];
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        if (c == '=') break;
        i32 v = value_of(c);
        if (v < 0) break;
        acc = (acc << 6) | (u32)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (n >= max) return -1;
            out[n++] = (u8)((acc >> bits) & 0xFF);
        }
    }
    return (i32)n;
}
