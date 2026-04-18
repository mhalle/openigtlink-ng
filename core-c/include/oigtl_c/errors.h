/* Error codes for the C codec.
 *
 * Zero means success. Negative means an error occurred; the
 * absolute value is a class of failure the caller can act on.
 *
 * Intentionally flat — no exception hierarchy, no error-object
 * allocation, no `errno`-style global. Every function that can fail
 * returns an `int` and documents the possible codes in its comment.
 */
#ifndef OIGTL_C_ERRORS_H
#define OIGTL_C_ERRORS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    OIGTL_OK                  =  0,

    /* Caller-supplied buffer was too small for the requested
     * operation. Typical source: pack() with a body larger than
     * the destination capacity, or unpack() with a wire payload
     * shorter than the declared body_size. */
    OIGTL_ERR_SHORT_BUFFER    = -1,

    /* Wire bytes are not valid for the claimed message type:
     * body_size that does not match any fixed or allowed variant,
     * a struct-array trailing remainder, etc. The exact reason is
     * not carried — embedded logs typically don't have room for
     * free-form strings. */
    OIGTL_ERR_MALFORMED       = -2,

    /* CRC-64 over the body did not match the value in the header.
     * Caller must decide whether to reject or accept. */
    OIGTL_ERR_CRC_MISMATCH    = -3,

    /* An input argument was NULL where a valid pointer was required,
     * or a length was zero where the operation cannot proceed. */
    OIGTL_ERR_INVALID_ARG     = -4,

    /* Field value out of the schema-allowed range (e.g. a device
     * name longer than 20 bytes, an encoding enum value not in the
     * allowed set). Pack-time equivalent of OIGTL_ERR_MALFORMED. */
    OIGTL_ERR_FIELD_RANGE     = -5,
};

#ifdef __cplusplus
}
#endif

#endif /* OIGTL_C_ERRORS_H */
