/*
 * This is the header file for the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#ifndef MD5_H
#define MD5_H

#include <cstdint>

struct MD5Context
{
	MD5Context() { Init(); }

	void Init();
	void Update(const uint8_t *buf, unsigned len);
	void Final(uint8_t digest[16]);

private:
	uint32_t buf[4];
	uint32_t bytes[2];
	uint32_t in[16];

};

void MD5Transform(uint32_t buf[4], uint32_t const in[16]);

#endif /* !MD5_H */
