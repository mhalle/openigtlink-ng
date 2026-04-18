/* CRC-64 ECMA-182 — the checksum used by the 58-byte IGTL header.
 *
 * Caller-facing API is deliberately minimal: compute the CRC of a
 * buffer, either in one pass (`oigtl_crc64`) or incrementally across
 * multiple chunks (`oigtl_crc64_update` + final value).
 *
 * Implementation uses a single 256-entry table (2 KiB of ROM). A
 * slice-by-8 variant would be faster on large buffers but costs
 * 16 KiB of ROM, which matters on small MCUs. If you're computing
 * CRC over multi-MB NDARRAY bodies and care about cycles, swap in
 * the slice-by-8 table — the single-table version is the right
 * default for embedded.
 */
#ifndef OIGTL_C_CRC64_H
#define OIGTL_C_CRC64_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compute the ECMA-182 CRC-64 of `len` bytes at `data`.
 * Returns 0 when `data` is NULL or `len` is 0 — matches the C++ and
 * Python runtimes' "empty body hashes to 0" convention. */
uint64_t oigtl_crc64(const uint8_t *data, size_t len);

/* Incremental variant. Start with `crc = 0`, feed successive chunks,
 * final value is the return of the last call. */
uint64_t oigtl_crc64_update(uint64_t crc, const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_CRC64_H */
