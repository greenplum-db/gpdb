/*-------------------------------------------------------------------------
 *
 * crypt.c
 *	  Look into the password file and check the encrypted password with
 *	  the one passed in from the frontend.
 *
 * Original coding by Todd A. Brandys
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/backend/libpq/crypt.c,v 1.77 2009/01/01 17:23:42 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "libpq/crypt.h"
#include "libpq/md5.h"
#include "libpq/password_hash.h"
#include "libpq/pg_sha2.h"

bool
hash_password(const char *passwd, char *salt, size_t salt_len, char *buf)
{
	switch (password_hash_algorithm)
	{
		case PASSWORD_HASH_MD5:
			return pg_md5_encrypt(passwd, salt, salt_len, buf);
		case PASSWORD_HASH_SHA_256:
			return pg_sha256_encrypt(passwd, salt, salt_len, buf);
			break;
		default:
			elog(ERROR,
				 "unknown password hash algorithm number %d",
				 password_hash_algorithm);
	}
	return false; /* we never get here */
}


int
hashed_passwd_verify(const Port *port, const char *role, char *client_pass)
{
	char	   *shadow_pass = NULL,
			   *valuntil = NULL,
			   *crypt_pwd;
	int			retval = STATUS_ERROR;
	List	  **line;
	ListCell   *token;
	char	   *crypt_client_pass = client_pass;

	if ((line = get_role_line(role)) == NULL)
		return STATUS_ERROR;

	/* Skip over rolename */
	token = list_head(*line);
	if (token)
		token = lnext(token);
	if (token)
	{
		shadow_pass = (char *) lfirst(token);
		token = lnext(token);
		if (token)
			valuntil = (char *) lfirst(token);
	}

	if (shadow_pass == NULL || *shadow_pass == '\0')
		return STATUS_ERROR;

	/*
	 * Compare with the encrypted or plain password depending on the
	 * authentication method being used for this connection.
	 */
	switch (port->hba->auth_method)
	{
		case uaMD5:
			crypt_pwd = palloc(MD5_PASSWD_LEN + 1);
			if (isMD5(shadow_pass))
			{
				/* stored password already encrypted, only do salt */
				if (!pg_md5_encrypt(shadow_pass + strlen("md5"),
									(char *) port->md5Salt,
									sizeof(port->md5Salt), crypt_pwd))
				{
					pfree(crypt_pwd);
					return STATUS_ERROR;
				}
			}
			else if (isSHA256(shadow_pass))
			{
				/* 
				 * Client supplied an MD5 hashed password but our password 
				 * is stored as SHA256 so we cannot compare the two.
				 */
				ereport(FATAL,
						(errcode(ERRCODE_INVALID_AUTHORIZATION_SPECIFICATION),
						 errmsg("MD5 authentication is not supported with "
								"SHA256 hashed passwords"),
						 errhint("Set an alternative authentication method "
								 "for this role in pg_hba.conf")));


			}
			else
			{
				/* stored password is plain, double-encrypt */
				char	   *crypt_pwd2 = palloc(MD5_PASSWD_LEN + 1);

				if (!pg_md5_encrypt(shadow_pass,
									port->user_name,
									strlen(port->user_name),
									crypt_pwd2))
				{
					pfree(crypt_pwd);
					pfree(crypt_pwd2);
					return STATUS_ERROR;
				}
				if (!pg_md5_encrypt(crypt_pwd2 + strlen("md5"),
									port->md5Salt,
									sizeof(port->md5Salt),
									crypt_pwd))
				{
					pfree(crypt_pwd);
					pfree(crypt_pwd2);
					return STATUS_ERROR;
				}
				pfree(crypt_pwd2);
			}
			break;
		default:
			if (isMD5(shadow_pass))
			{
				/* Encrypt user-supplied password to match stored MD5 */
				crypt_client_pass = palloc(MD5_PASSWD_LEN + 1);
				if (!pg_md5_encrypt(client_pass,
									port->user_name,
									strlen(port->user_name),
									crypt_client_pass))
				{
					pfree(crypt_client_pass);
					return STATUS_ERROR;
				}
			}
			else if (isSHA256(shadow_pass))
			{
				/* Encrypt user-supplied password to match the stored SHA-256 */
				crypt_client_pass = palloc(SHA256_PASSWD_LEN + 1);
				if (!pg_sha256_encrypt(client_pass,
									   port->user_name,
									   strlen(port->user_name),
									   crypt_client_pass))
				{
					pfree(crypt_client_pass);
					return STATUS_ERROR;
				}
			}
			crypt_pwd = shadow_pass;
			break;
	}
	if (strcmp(crypt_client_pass, crypt_pwd) == 0)
	{
		/*
		 * Password OK, now check to be sure we are not past valuntil
		 */
		if (valuntil == NULL || *valuntil == '\0')
			retval = STATUS_OK;
		else
		{
			TimestampTz vuntil;

			vuntil = DatumGetTimestampTz(DirectFunctionCall3(timestamptz_in,
												   CStringGetDatum(valuntil),
												ObjectIdGetDatum(InvalidOid),
														 Int32GetDatum(-1)));

			if (vuntil < GetCurrentTimestamp())
				retval = STATUS_ERROR;
			else
				retval = STATUS_OK;
		}
	}

	if (port->hba->auth_method == uaMD5)
		pfree(crypt_pwd);
	if (crypt_client_pass != client_pass)
		pfree(crypt_client_pass);

	return retval;
}
