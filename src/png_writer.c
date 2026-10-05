#include "png_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- CRC32 ----

static uint32_t g_crc_table[256];
static bool g_crc_ready = false;

static void crc32_init(void)
{
    for (uint32_t n = 0; n < 256; n++)
    {
        uint32_t c = n;
        for (int k = 0; k < 8; k++)
        {
            c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        }
        g_crc_table[n] = c;
    }
    g_crc_ready = true;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t * buf, size_t len)
{
    if (!g_crc_ready)
    {
        crc32_init();
    }
    for (size_t i = 0; i < len; i++)
    {
        crc = g_crc_table[(crc ^ buf[i]) & 0xFFu] ^ (crc >> 8);
    }
    return crc;
}

// ---- Adler32 ----

static uint32_t adler32_update(uint32_t adler, const uint8_t * buf, size_t len)
{
    uint32_t s1 = adler & 0xFFFFu;
    uint32_t s2 = (adler >> 16) & 0xFFFFu;
    for (size_t i = 0; i < len; i++)
    {
        s1 = (s1 + buf[i]) % 65521u;
        s2 = (s2 + s1) % 65521u;
    }
    return (s2 << 16) | s1;
}

// ---- Вспомогательное ----

static void write_u32_be(FILE * f, uint32_t v)
{
    fputc((int)((v >> 24) & 0xFFu), f);
    fputc((int)((v >> 16) & 0xFFu), f);
    fputc((int)((v >> 8) & 0xFFu), f);
    fputc((int)(v & 0xFFu), f);
}

static void write_chunk(FILE * f, const char * type, const uint8_t * data, uint32_t len)
{
    write_u32_be(f, len);

    // CRC считается по type + data
    uint32_t crc = 0xFFFFFFFFu;
    crc          = crc32_update(crc, (const uint8_t *)type, 4);
    if (data && len > 0)
    {
        crc = crc32_update(crc, data, len);
    }
    crc ^= 0xFFFFFFFFu;

    fwrite(type, 1, 4, f);
    if (data && len > 0)
    {
        fwrite(data, 1, len, f);
    }
    write_u32_be(f, crc);
}

// ---- Public ----

bool png_write_rgba(const char * path, const uint8_t * rgba, int width, int height)
{
    if (!path || !rgba || width <= 0 || height <= 0)
    {
        return false;
    }

    FILE * f = fopen(path, "wb");
    if (!f)
    {
        return false;
    }

    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(sig, 1, 8, f);

    // IHDR
    uint8_t ihdr[13];
    ihdr[0]  = (uint8_t)((width >> 24) & 0xFFu);
    ihdr[1]  = (uint8_t)((width >> 16) & 0xFFu);
    ihdr[2]  = (uint8_t)((width >> 8) & 0xFFu);
    ihdr[3]  = (uint8_t)(width & 0xFFu);
    ihdr[4]  = (uint8_t)((height >> 24) & 0xFFu);
    ihdr[5]  = (uint8_t)((height >> 16) & 0xFFu);
    ihdr[6]  = (uint8_t)((height >> 8) & 0xFFu);
    ihdr[7]  = (uint8_t)(height & 0xFFu);
    ihdr[8]  = 8; // bit depth
    ihdr[9]  = 6; // color type RGBA
    ihdr[10] = 0; // compression: deflate
    ihdr[11] = 0; // filter method
    ihdr[12] = 0; // interlace
    write_chunk(f, "IHDR", ihdr, 13);

    // Готовим raw-строки: filter byte (0 = None) + RGBA пиксели
    size_t stride  = (size_t)width * 4;
    size_t raw_len = (size_t)height * (stride + 1);
    uint8_t * raw  = malloc(raw_len);
    if (!raw)
    {
        fclose(f);
        return false;
    }

    for (int y = 0; y < height; y++)
    {
        raw[(size_t)y * (stride + 1)] = 0;
        memcpy(raw + ((size_t)y * (stride + 1)) + 1, rgba + ((size_t)y * stride), stride);
    }

    // zlib-поток: 2 байта заголовка + deflate stored-блоки + Adler32
    size_t max_blocks = (raw_len / 65535) + 2;
    size_t zlib_cap   = 2 + raw_len + (max_blocks * 5) + 4;
    uint8_t * zlib    = malloc(zlib_cap);
    if (!zlib)
    {
        free(raw);
        fclose(f);
        return false;
    }
    size_t zpos = 0;

    // CMF=0x78 (deflate, 32K), FLG=0x01
    zlib[zpos++] = 0x78;
    zlib[zpos++] = 0x01;

    size_t pos = 0;
    do
    {
        size_t remaining = raw_len - pos;
        size_t block     = remaining > 65535 ? 65535 : remaining;
        bool final       = (pos + block >= raw_len);

        zlib[zpos++] = final ? 0x01 : 0x00; // BFINAL, BTYPE=00 (stored)
        zlib[zpos++] = (uint8_t)(block & 0xFFu);
        zlib[zpos++] = (uint8_t)((block >> 8) & 0xFFu);

        uint16_t nlen = (uint16_t)(~block);
        zlib[zpos++]  = (uint8_t)(nlen & 0xFFu);
        zlib[zpos++]  = (uint8_t)((nlen >> 8) & 0xFFu);

        if (block > 0)
        {
            memcpy(zlib + zpos, raw + pos, block);
            zpos += block;
        }
        pos += block;
    } while (pos < raw_len);

    uint32_t adler = adler32_update(1u, raw, raw_len);
    zlib[zpos++]   = (uint8_t)((adler >> 24) & 0xFFu);
    zlib[zpos++]   = (uint8_t)((adler >> 16) & 0xFFu);
    zlib[zpos++]   = (uint8_t)((adler >> 8) & 0xFFu);
    zlib[zpos++]   = (uint8_t)(adler & 0xFFu);

    write_chunk(f, "IDAT", zlib, (uint32_t)zpos);
    write_chunk(f, "IEND", nullptr, 0);

    free(zlib);
    free(raw);
    fclose(f);
    return true;
}
