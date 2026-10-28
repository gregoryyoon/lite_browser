// Copyright (c) 2026 Lite Browser. All rights reserved.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_GZIP_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_GZIP_H_

#ifdef __cplusplus
extern "C" {
#endif

// Returns 1 if the file exists and begins with gzip magic bytes (0x1F, 0x8B).
__declspec(dllexport) int simple_is_gzip_file(const char *utf8_path);

// Decompresses a gzip-compressed file in-place to pure uncompressed text/binary.
// Returns 1 on success, 0 on failure.
__declspec(dllexport) int simple_decompress_gzip_file_inplace(const char *utf8_path);

#ifdef __cplusplus
}
#endif

#endif // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_GZIP_H_
