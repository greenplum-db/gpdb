/*-------------------------------------------------------------------------
 *
 * crypt.c
 *	  Look into the password file and check the encrypted password with
 *	  the one passed in from the frontend.
 *
 * Original coding by Todd A. Brandys
 *
 * Portions Copyright (c) 1996-2015, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/backend/libpq/crypt.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>
#ifdef HAVE_CRYPT_H
#include <crypt.h>
#endif

#include "catalog/pg_authid.h"
#include "libpq/crypt.h"
#include "libpq/md5.h"
#include "libpq/password_hash.h"
#include "libpq/pg_sha2.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

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


/*
 * Check given password for given user, and return STATUS_OK or STATUS_ERROR.
 * In the error case, optionally store a palloc'd string at *logdetail
 * that will be sent to the postmaster log (but not the client).
 */
int
hashed_passwd_verify(const Port *port, const char *role, char *client_pass,
				 char **logdetail)
{
	int			retval = STATUS_ERROR;
	char	   *shadow_pass,
			   *crypt_pwd;
	TimestampTz vuntil = 0;
	char	   *crypt_client_pass = client_pass;
	HeapTuple	roleTup;
	Datum		datum;
	bool		isnull;

	/* Get role info from pg_authid */
	roleTup = SearchSysCache1(AUTHNAME, PointerGetDatum(role));
	if (!HeapTupleIsValid(roleTup))
		return STATUS_ERROR;	/* no such user */

	datum = SysCacheGetAttr(AUTHNAME, roleTup,
							Anum_pg_authid_rolpassword, &isnull);
	if (isnull)
	{
		ReleaseSysCache(roleTup);
		*logdetail = psprintf(_("User \"%s\" has no password assigned."),
							  role);
		return STATUS_ERROR;	/* user has no password */
	}
	shadow_pass = TextDatumGetCString(datum);

	datum = SysCacheGetAttr(AUTHNAME, roleTup,
							Anum_pg_authid_rolvaliduntil, &isnull);
	if (!isnull)
		vuntil = DatumGetTimestampTz(datum);

	ReleaseSysCache(roleTup);

	CHECK_FOR_INTERRUPTS();

	/*
	 * Don't allow an empty password. Libpq treats an empty password the same
	 * as no password at all, and won't even try to authenticate. But other
	 * clients might, so allowing it would be confusing.
	 *
	 * For a plaintext password, we can simply check that it's not an empty
	 * string. For an encrypted password, check that it does not match the MD5
	 * hash of an empty string.
	 */
	if (*shadow_pass == '\0')
	{
		*logdetail = psprintf(_("User \"%s\" has an empty password."),
							  role);
		return STATUS_ERROR;	/* empty password */
	}
	if (isMD5(shadow_pass))
	{
		char		crypt_empty[MD5_PASSWD_LEN + 1];

		if (!pg_md5_encrypt("",
							port->user_name,
							strlen(port->user_name),
							crypt_empty))
			return STATUS_ERROR;
		if (strcmp(shadow_pass, crypt_empty) == 0)
		{
			*logdetail = psprintf(_("User \"%s\" has an empty password."),
								  role);
			return STATUS_ERROR;	/* empty password */
		}
	}

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
									port->md5Salt,
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
		 * Password OK, now check to be sure we are not past rolvaliduntil
		 */
		if (isnull)
			retval = STATUS_OK;
		else if (vuntil < GetCurrentTimestamp())
		{
			*logdetail = psprintf(_("User \"%s\" has an expired password."),
								  role);
			retval = STATUS_ERROR;
		}
		else
			retval = STATUS_OK;
	}

	if (port->hba->auth_method == uaMD5)
		pfree(crypt_pwd);
	if (crypt_client_pass != client_pass)
		pfree(crypt_client_pass);

	return retval;
}
