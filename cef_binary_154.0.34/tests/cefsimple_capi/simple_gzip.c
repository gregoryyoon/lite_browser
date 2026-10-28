// Copyright (c) 2026 Lite Browser. All rights reserved.
// Embedded lightweight gzip decompressor based on tinf (Tiny Inflate library by Joergen Ibsen).

#include "tests/cefsimple_capi/simple_gzip.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#if defined(UINT_MAX) && (UINT_MAX) < 0xFFFFFFFFUL
#  error "simple_gzip requires unsigned int to be at least 32-bit"
#endif

// Status codes returned by internal decompressor
enum {
  TINF_OK         = 0,
  TINF_DATA_ERROR = -3,
  TINF_BUF_ERROR  = -5
};

typedef enum {
  FTEXT    = 1,
  FHCRC    = 2,
  FEXTRA   = 4,
  FNAME    = 8,
  FCOMMENT = 16
} tinf_gzip_flag;

struct tinf_tree {
  unsigned short counts[16];   // Number of codes with a given length
  unsigned short symbols[288];  // Symbols sorted by code
  int max_sym;
};

struct tinf_data {
  const unsigned char *source;
  const unsigned char *source_end;
  unsigned int tag;
  int bitcount;
  int overflow;

  unsigned char *dest_start;
  unsigned char *dest;
  unsigned char *dest_end;

  struct tinf_tree ltree; // Literal/length tree
  struct tinf_tree dtree; // Distance tree
};

// CRC32 table
static const unsigned int tinf_crc32tab[16] = {
  0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190,
  0x6B6B51F4, 0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344,
  0xD6D6A3E8, 0xCB61B38C, 0x9B64C2B0, 0x86D3D2D4, 0xA00AE278,
  0xBDBDF21C
};

static unsigned int tinf_crc32(const void *data, unsigned int length) {
  const unsigned char *buf = (const unsigned char *)data;
  unsigned int crc = 0xFFFFFFFF;
  if (length == 0) return 0;
  for (unsigned int i = 0; i < length; ++i) {
    crc ^= buf[i];
    crc = tinf_crc32tab[crc & 0x0F] ^ (crc >> 4);
    crc = tinf_crc32tab[crc & 0x0F] ^ (crc >> 4);
  }
  return crc ^ 0xFFFFFFFF;
}

static unsigned int read_le16(const unsigned char *p) {
  return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8);
}

static unsigned int read_le32(const unsigned char *p) {
  return ((unsigned int)p[0])
       | ((unsigned int)p[1] << 8)
       | ((unsigned int)p[2] << 16)
       | ((unsigned int)p[3] << 24);
}

static void tinf_build_fixed_trees(struct tinf_tree *lt, struct tinf_tree *dt) {
  int i;
  for (i = 0; i < 16; ++i) lt->counts[i] = 0;
  lt->counts[7] = 24;
  lt->counts[8] = 152;
  lt->counts[9] = 112;
  for (i = 0; i < 24; ++i) lt->symbols[i] = 256 + i;
  for (i = 0; i < 144; ++i) lt->symbols[24 + i] = i;
  for (i = 0; i < 8; ++i) lt->symbols[24 + 144 + i] = 280 + i;
  for (i = 0; i < 112; ++i) lt->symbols[24 + 144 + 8 + i] = 144 + i;
  lt->max_sym = 285;

  for (i = 0; i < 16; ++i) dt->counts[i] = 0;
  dt->counts[5] = 32;
  for (i = 0; i < 32; ++i) dt->symbols[i] = i;
  dt->max_sym = 29;
}

static int tinf_build_tree(struct tinf_tree *t, const unsigned char *lengths, unsigned int num) {
  unsigned short offs[16];
  unsigned int i, num_codes, available;
  assert(num <= 288);
  for (i = 0; i < 16; ++i) t->counts[i] = 0;
  t->max_sym = -1;
  for (i = 0; i < num; ++i) {
    assert(lengths[i] <= 15);
    if (lengths[i]) {
      t->max_sym = i;
      t->counts[lengths[i]]++;
    }
  }
  for (available = 1, num_codes = 0, i = 0; i < 16; ++i) {
    unsigned int used = t->counts[i];
    if (used > available) return TINF_DATA_ERROR;
    available = 2 * (available - used);
    offs[i] = num_codes;
    num_codes += used;
  }
  if ((num_codes > 1 && available > 0) || (num_codes == 1 && t->counts[1] != 1)) {
    return TINF_DATA_ERROR;
  }
  for (i = 0; i < num; ++i) {
    if (lengths[i]) t->symbols[offs[lengths[i]]++] = i;
  }
  if (num_codes == 1) {
    t->counts[1] = 2;
    t->symbols[1] = t->max_sym + 1;
  }
  return TINF_OK;
}

static void tinf_refill(struct tinf_data *d, int num) {
  assert(num >= 0 && num <= 32);
  while (d->bitcount < num) {
    if (d->source != d->source_end) {
      d->tag |= (unsigned int)*d->source++ << d->bitcount;
    } else {
      d->overflow = 1;
    }
    d->bitcount += 8;
  }
  assert(d->bitcount <= 32);
}

static unsigned int tinf_getbits_no_refill(struct tinf_data *d, int num) {
  unsigned int bits;
  assert(num >= 0 && num <= d->bitcount);
  bits = d->tag & ((1UL << num) - 1);
  d->tag >>= num;
  d->bitcount -= num;
  return bits;
}

static unsigned int tinf_getbits(struct tinf_data *d, int num) {
  tinf_refill(d, num);
  return tinf_getbits_no_refill(d, num);
}

static unsigned int tinf_getbits_base(struct tinf_data *d, int num, int base) {
  return base + (num ? tinf_getbits(d, num) : 0);
}

static int tinf_decode_symbol(struct tinf_data *d, const struct tinf_tree *t) {
  int base = 0, offs = 0;
  int len;
  for (len = 1; ; ++len) {
    offs = 2 * offs + tinf_getbits(d, 1);
    assert(len <= 15);
    if (offs < t->counts[len]) break;
    base += t->counts[len];
    offs -= t->counts[len];
  }
  assert(base + offs >= 0 && base + offs < 288);
  return t->symbols[base + offs];
}

static int tinf_decode_trees(struct tinf_data *d, struct tinf_tree *lt, struct tinf_tree *dt) {
  unsigned char lengths[288 + 32];
  static const unsigned char clcidx[19] = {
    16, 17, 18, 0,  8, 7,  9, 6, 10, 5,
    11,  4, 12, 3, 13, 2, 14, 1, 15
  };
  unsigned int hlit, hdist, hclen;
  unsigned int i, num, length;
  int res;

  hlit = tinf_getbits_base(d, 5, 257);
  hdist = tinf_getbits_base(d, 5, 1);
  hclen = tinf_getbits_base(d, 4, 4);

  if (hlit > 286 || hdist > 30) return TINF_DATA_ERROR;

  for (i = 0; i < 19; ++i) lengths[i] = 0;
  for (i = 0; i < hclen; ++i) {
    unsigned int clen = tinf_getbits(d, 3);
    lengths[clcidx[i]] = clen;
  }
  res = tinf_build_tree(lt, lengths, 19);
  if (res != TINF_OK) return res;
  if (lt->max_sym == -1) return TINF_DATA_ERROR;

  for (num = 0; num < hlit + hdist; ) {
    int sym = tinf_decode_symbol(d, lt);
    if (sym > lt->max_sym) return TINF_DATA_ERROR;
    switch (sym) {
    case 16:
      if (num == 0) return TINF_DATA_ERROR;
      sym = lengths[num - 1];
      length = tinf_getbits_base(d, 2, 3);
      break;
    case 17:
      sym = 0;
      length = tinf_getbits_base(d, 3, 3);
      break;
    case 18:
      sym = 0;
      length = tinf_getbits_base(d, 7, 11);
      break;
    default:
      length = 1;
      break;
    }
    if (length > hlit + hdist - num) return TINF_DATA_ERROR;
    while (length--) lengths[num++] = sym;
  }

  if (lengths[256] == 0) return TINF_DATA_ERROR;
  res = tinf_build_tree(lt, lengths, hlit);
  if (res != TINF_OK) return res;
  res = tinf_build_tree(dt, lengths + hlit, hdist);
  if (res != TINF_OK) return res;
  return TINF_OK;
}

static int tinf_inflate_block_data(struct tinf_data *d, struct tinf_tree *lt, struct tinf_tree *dt) {
  static const unsigned char length_bits[30] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
    4, 4, 4, 4, 5, 5, 5, 5, 0, 127
  };
  static const unsigned short length_base[30] = {
     3,  4,  5,   6,   7,   8,   9,  10,  11,  13,
    15, 17, 19,  23,  27,  31,  35,  43,  51,  59,
    67, 83, 99, 115, 131, 163, 195, 227, 258,   0
  };
  static const unsigned char dist_bits[30] = {
    0, 0,  0,  0,  1,  1,  2,  2,  3,  3,
    4, 4,  5,  5,  6,  6,  7,  7,  8,  8,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13
  };
  static const unsigned short dist_base[30] = {
       1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
      33,   49,   65,   97,  129,  193,  257,   385,   513,   769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
  };

  for (;;) {
    int sym = tinf_decode_symbol(d, lt);
    if (d->overflow) return TINF_DATA_ERROR;
    if (sym < 256) {
      if (d->dest == d->dest_end) return TINF_BUF_ERROR;
      *d->dest++ = sym;
    } else {
      int length, dist, offs;
      int i;
      if (sym == 256) return TINF_OK;
      if (sym > lt->max_sym || sym - 257 > 28 || dt->max_sym == -1) return TINF_DATA_ERROR;
      sym -= 257;
      length = tinf_getbits_base(d, length_bits[sym], length_base[sym]);
      dist = tinf_decode_symbol(d, dt);
      if (dist > dt->max_sym || dist > 29) return TINF_DATA_ERROR;
      offs = tinf_getbits_base(d, dist_bits[dist], dist_base[dist]);
      if (offs > d->dest - d->dest_start) return TINF_DATA_ERROR;
      if (d->dest_end - d->dest < length) return TINF_BUF_ERROR;
      for (i = 0; i < length; ++i) {
        d->dest[i] = d->dest[i - offs];
      }
      d->dest += length;
    }
  }
}

static int tinf_inflate_uncompressed_block(struct tinf_data *d) {
  unsigned int length, invlength;
  if (d->source_end - d->source < 4) return TINF_DATA_ERROR;
  length = read_le16(d->source);
  invlength = read_le16(d->source + 2);
  if (length != (~invlength & 0x0000FFFF)) return TINF_DATA_ERROR;
  d->source += 4;
  if (d->source_end - d->source < length) return TINF_DATA_ERROR;
  if (d->dest_end - d->dest < length) return TINF_BUF_ERROR;
  while (length--) *d->dest++ = *d->source++;
  d->tag = 0;
  d->bitcount = 0;
  return TINF_OK;
}

static int tinf_inflate_fixed_block(struct tinf_data *d) {
  tinf_build_fixed_trees(&d->ltree, &d->dtree);
  return tinf_inflate_block_data(d, &d->ltree, &d->dtree);
}

static int tinf_inflate_dynamic_block(struct tinf_data *d) {
  int res = tinf_decode_trees(d, &d->ltree, &d->dtree);
  if (res != TINF_OK) return res;
  return tinf_inflate_block_data(d, &d->ltree, &d->dtree);
}

static int tinf_uncompress(void *dest, unsigned int *destLen,
                           const void *source, unsigned int sourceLen) {
  struct tinf_data d;
  int bfinal;
  d.source = (const unsigned char *)source;
  d.source_end = d.source + sourceLen;
  d.tag = 0;
  d.bitcount = 0;
  d.overflow = 0;
  d.dest = (unsigned char *)dest;
  d.dest_start = d.dest;
  d.dest_end = d.dest + *destLen;

  do {
    unsigned int btype;
    int res;
    bfinal = tinf_getbits(&d, 1);
    btype = tinf_getbits(&d, 2);
    switch (btype) {
    case 0: res = tinf_inflate_uncompressed_block(&d); break;
    case 1: res = tinf_inflate_fixed_block(&d); break;
    case 2: res = tinf_inflate_dynamic_block(&d); break;
    default: res = TINF_DATA_ERROR; break;
    }
    if (res != TINF_OK) return res;
  } while (!bfinal);

  if (d.overflow) return TINF_DATA_ERROR;
  *destLen = (unsigned int)(d.dest - d.dest_start);
  return TINF_OK;
}

static int tinf_gzip_uncompress(void *dest, unsigned int *destLen,
                                const void *source, unsigned int sourceLen) {
  const unsigned char *src = (const unsigned char *)source;
  unsigned char *dst = (unsigned char *)dest;
  const unsigned char *start;
  unsigned int dlen, crc32;
  int res;
  unsigned char flg;

  if (sourceLen < 18) return TINF_DATA_ERROR;
  if (src[0] != 0x1F || src[1] != 0x8B) return TINF_DATA_ERROR;
  if (src[2] != 8) return TINF_DATA_ERROR;
  flg = src[3];
  if (flg & 0xE0) return TINF_DATA_ERROR;

  start = src + 10;
  if (flg & FEXTRA) {
    unsigned int xlen = read_le16(start);
    if (xlen > sourceLen - 12) return TINF_DATA_ERROR;
    start += xlen + 2;
  }
  if (flg & FNAME) {
    do {
      if (start - src >= (ptrdiff_t)sourceLen) return TINF_DATA_ERROR;
    } while (*start++);
  }
  if (flg & FCOMMENT) {
    do {
      if (start - src >= (ptrdiff_t)sourceLen) return TINF_DATA_ERROR;
    } while (*start++);
  }
  if (flg & FHCRC) {
    unsigned int hcrc;
    if (start - src > (ptrdiff_t)sourceLen - 2) return TINF_DATA_ERROR;
    hcrc = read_le16(start);
    if (hcrc != (tinf_crc32(src, (unsigned int)(start - src)) & 0x0000FFFF)) return TINF_DATA_ERROR;
    start += 2;
  }

  dlen = read_le32(&src[sourceLen - 4]);
  if (dlen > *destLen) return TINF_BUF_ERROR;

  crc32 = read_le32(&src[sourceLen - 8]);
  if ((src + sourceLen) - start < 8) return TINF_DATA_ERROR;

  res = tinf_uncompress(dst, destLen, start, (unsigned int)((src + sourceLen) - start - 8));
  if (res != TINF_OK) return TINF_DATA_ERROR;
  if (*destLen != dlen) return TINF_DATA_ERROR;

  if (crc32 != tinf_crc32(dst, dlen)) return TINF_DATA_ERROR;
  return TINF_OK;
}

// Public API implementation

int simple_is_gzip_file(const char *utf8_path) {
  if (!utf8_path || utf8_path[0] == '\0') return 0;
  WCHAR wpath[MAX_PATH * 2] = {0};
  if (MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, wpath, MAX_PATH * 2) <= 0) {
    return 0;
  }
  FILE *f = NULL;
  if (_wfopen_s(&f, wpath, L"rb") != 0 || !f) {
    return 0;
  }
  unsigned char magic[2] = {0};
  size_t read_bytes = fread(magic, 1, 2, f);
  fclose(f);
  return (read_bytes == 2 && magic[0] == 0x1F && magic[1] == 0x8B);
}

int simple_decompress_gzip_file_inplace(const char *utf8_path) {
  if (!utf8_path || utf8_path[0] == '\0') return 0;
  WCHAR wpath[MAX_PATH * 2] = {0};
  if (MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, wpath, MAX_PATH * 2) <= 0) {
    return 0;
  }

  FILE *f = NULL;
  if (_wfopen_s(&f, wpath, L"rb") != 0 || !f) {
    return 0;
  }

  _fseeki64(f, 0, SEEK_END);
  int64_t file_size = _ftelli64(f);
  _fseeki64(f, 0, SEEK_SET);

  if (file_size < 18 || file_size > 500 * 1024 * 1024) {
    fclose(f);
    return 0;
  }

  unsigned char *src = (unsigned char *)malloc((size_t)file_size);
  if (!src) {
    fclose(f);
    return 0;
  }

  size_t read_len = fread(src, 1, (size_t)file_size, f);
  fclose(f);

  if (read_len != (size_t)file_size || src[0] != 0x1F || src[1] != 0x8B) {
    free(src);
    return 0;
  }

  unsigned int dlen = read_le32(&src[file_size - 4]);
  unsigned int alloc_size = dlen;
  if (alloc_size == 0) {
    alloc_size = 1;
  }

  // Prevent memory exhaustion on malformed/huge streams (> 1GB)
  if (alloc_size > 1024 * 1024 * 1024) {
    free(src);
    return 0;
  }

  unsigned char *dst = (unsigned char *)malloc(alloc_size + 1);
  if (!dst) {
    free(src);
    return 0;
  }

  unsigned int destLen = dlen;
  int res = tinf_gzip_uncompress(dst, &destLen, src, (unsigned int)file_size);
  free(src);

  if (res != TINF_OK) {
    free(dst);
    return 0;
  }

  // Write decompressed data to temporary file, then atomically replace original file
  WCHAR wtmp_path[MAX_PATH * 2] = {0};
  swprintf_s(wtmp_path, MAX_PATH * 2, L"%s.decomp.tmp", wpath);

  FILE *out_f = NULL;
  if (_wfopen_s(&out_f, wtmp_path, L"wb") != 0 || !out_f) {
    free(dst);
    return 0;
  }

  if (destLen > 0) {
    fwrite(dst, 1, destLen, out_f);
  }
  fclose(out_f);
  free(dst);

  // Replace original file with decompressed file (with retry for transient locks)
  int replaced = 0;
  for (int retry = 0; retry < 5; ++retry) {
    if (ReplaceFileW(wpath, wtmp_path, NULL, 0, NULL, NULL)) {
      replaced = 1;
      break;
    }
    Sleep(30);
  }

  if (!replaced) {
    for (int retry = 0; retry < 5; ++retry) {
      if (DeleteFileW(wpath) || GetLastError() == ERROR_FILE_NOT_FOUND) {
        if (MoveFileExW(wtmp_path, wpath, MOVEFILE_REPLACE_EXISTING)) {
          replaced = 1;
          break;
        }
      }
      Sleep(30);
    }
  }

  if (!replaced) {
    DeleteFileW(wtmp_path);
    return 0;
  }

  return 1;
}
