/*-------------------------------------------------------------------------
 *
 * password_hash.h
 *
 * Declarations and constants for password hashing.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PASSWORD_HASH_H
#define PASSWORD_HASH_H

#include "libpq/md5.h"
#include "libpq/pg_sha2.h"

#define isHashedPasswd(passwd) (isMD5(passwd) || isSHA256(passwd))

#define MAX_PASSWD_HASH_LEN Max(MD5_PASSWD_LEN, SHA256_PASSWD_LEN)

extern bool hash_password(const char *passwd, char *salt, size_t salt_len,
						  char *buf);

typedef enum
{
	PASSWORD_HASH_NONE = 0,
	PASSWORD_HASH_MD5,
	PASSWORD_HASH_SHA_256
} PasswdHashAlg;

extern int password_hash_algorithm;

#endif
