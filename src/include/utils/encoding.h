#ifndef _ENCODING_H_
#define _ENCODING_H_
/* Provides encoding and decoding to convert the estimator bytes into a human
 * readable form. Currently only base 64 encoding is provided. */

int hll_b64_encode(const char *src, unsigned len, char *dst);
int hll_b64_decode(const char *src, unsigned len, char *dst);
int b64_enc_len(const char *src, unsigned srclen);
int b64_dec_len(const char *src, unsigned srclen);

#endif // #ifndef _ENCODING_H_
