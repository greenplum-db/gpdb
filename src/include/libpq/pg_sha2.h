#define SHA256_PREFIX "sha256"

#define SHA256_PASSWD_LEN strlen(SHA256_PREFIX) + 64

extern bool pg_sha256_encrypt(const char *pass, char *salt, size_t salt_len,
							  char *cryptpass);
