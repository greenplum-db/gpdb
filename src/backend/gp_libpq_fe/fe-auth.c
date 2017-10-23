/*-------------------------------------------------------------------------
 *
 * fe-auth.c
 *	   The front-end (client) authorization routines
 *
 * Portions Copyright (c) 1996-2010, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  $PostgreSQL: pgsql/src/interfaces/libpq/fe-auth.c,v 1.144.4.1 2010/07/14 17:09:54 tgl Exp $
 *
 *-------------------------------------------------------------------------
 */

/*
 * INTERFACE ROUTINES
 *	   frontend (client) routines:
 *		pg_fe_sendauth			send authentication information
 *		pg_fe_getauthname		get user's name according to the client side
 *								of the authentication system
 */

#include "postgres.h"

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>			/* for MAXHOSTNAMELEN on most */
#include <sys/socket.h>
#if defined(HAVE_STRUCT_CMSGCRED) || defined(HAVE_STRUCT_FCRED) || defined(HAVE_STRUCT_SOCKCRED)
#include <sys/uio.h>
#include <sys/ucred.h>
#endif
#ifndef  MAXHOSTNAMELEN
#include <netdb.h>				/* for MAXHOSTNAMELEN on some */
#endif
#include <pwd.h>
#endif

#ifdef HAVE_CRYPT_H
/* Is this include obsolete?  */
#include <crypt.h>
#endif

#include "gp-libpq-fe.h"
#include "gp-fe-auth.h"
#include "libpq/md5.h"

#include "gp-libpq-use.h"

/*
 * Respond to AUTH_REQ_SCM_CREDS challenge.
 *
 * Note: current backends will not use this challenge if HAVE_GETPEEREID
 * or SO_PEERCRED is defined, but pre-7.4 backends might, so compile the
 * code anyway.
 */
static int
pg_local_sendauth(PGconn *conn)
{
#if defined(HAVE_STRUCT_CMSGCRED) || defined(HAVE_STRUCT_FCRED) || \
	(defined(HAVE_STRUCT_SOCKCRED) && defined(LOCAL_CREDS))
	char		buf;
	struct iovec iov;
	struct msghdr msg;

#ifdef HAVE_STRUCT_CMSGCRED
	/* Prevent padding */
	char		cmsgmem[sizeof(struct cmsghdr) + sizeof(struct cmsgcred)];

	/* Point to start of first structure */
	struct cmsghdr *cmsg = (struct cmsghdr *) cmsgmem;
#endif

	/*
	 * The backend doesn't care what we send here, but it wants exactly one
	 * character to force recvmsg() to block and wait for us.
	 */
	buf = '\0';
	iov.iov_base = &buf;
	iov.iov_len = 1;

	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

#ifdef HAVE_STRUCT_CMSGCRED
	/* Create control header, FreeBSD */
	msg.msg_control = cmsg;
	msg.msg_controllen = sizeof(cmsgmem);
	memset(cmsg, 0, sizeof(cmsgmem));
	cmsg->cmsg_len = sizeof(cmsgmem);
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_CREDS;
#endif

	if (sendmsg(conn->sock, &msg, 0) == -1)
	{
		char		sebuf[256];

		printfPQExpBuffer(&conn->errorMessage,
						  "pg_local_sendauth: sendmsg: %s\n",
						  pqStrerror(errno, sebuf, sizeof(sebuf)));
		return STATUS_ERROR;
	}
	return STATUS_OK;
#else
	printfPQExpBuffer(&conn->errorMessage,
			libpq_gettext("SCM_CRED authentication method not supported\n"));
	return STATUS_ERROR;
#endif
}

static int
pg_password_sendauth(PGconn *conn, const char *password, AuthRequest areq)
{
	int			ret;
	char	   *crypt_pwd;

	/* Encrypt the password if needed. */

	switch (areq)
	{
		case AUTH_REQ_MD5:
			{
				char	   *crypt_pwd2;

				/* Allocate enough space for two MD5 hashes */
				crypt_pwd = malloc(2 * (MD5_PASSWD_LEN + 1));
				if (!crypt_pwd)
				{
					printfPQExpBuffer(&conn->errorMessage,
									  libpq_gettext("out of memory\n"));
					return STATUS_ERROR;
				}

				crypt_pwd2 = crypt_pwd + MD5_PASSWD_LEN + 1;
				if (!pg_md5_encrypt(password, conn->pguser,
									strlen(conn->pguser), crypt_pwd2))
				{
					free(crypt_pwd);
					return STATUS_ERROR;
				}
				if (!pg_md5_encrypt(crypt_pwd2 + strlen("md5"), conn->md5Salt,
									sizeof(conn->md5Salt), crypt_pwd))
				{
					free(crypt_pwd);
					return STATUS_ERROR;
				}
				break;
			}
		case AUTH_REQ_PASSWORD:
			/* discard const so we can assign it */
			crypt_pwd = (char *) password;
			break;
		default:
			return STATUS_ERROR;
	}
	/* Packet has a message type as of protocol 3.0 */
	if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
		ret = pqPacketSend(conn, 'p', crypt_pwd, strlen(crypt_pwd) + 1);
	else
		ret = pqPacketSend(conn, 0, crypt_pwd, strlen(crypt_pwd) + 1);
	if (areq == AUTH_REQ_MD5)
		free(crypt_pwd);
	return ret;
}

/*
 * pg_fe_sendauth
 *		client demux routine for outgoing authentication information
 */
int
pg_fe_sendauth(AuthRequest areq, PGconn *conn)
{
	switch (areq)
	{
		case AUTH_REQ_OK:
			break;

		case AUTH_REQ_KRB4:
			printfPQExpBuffer(&conn->errorMessage,
				 libpq_gettext("Kerberos 4 authentication not supported\n"));
			return STATUS_ERROR;

		case AUTH_REQ_KRB5:
			printfPQExpBuffer(&conn->errorMessage,
				 libpq_gettext("Kerberos 5 authentication not supported\n"));
			return STATUS_ERROR;

		case AUTH_REQ_GSS:
		case AUTH_REQ_GSS_CONT:
			printfPQExpBuffer(&conn->errorMessage,
					 libpq_gettext("GSSAPI authentication not supported\n"));
			return STATUS_ERROR;

		case AUTH_REQ_SSPI:
			printfPQExpBuffer(&conn->errorMessage,
					   libpq_gettext("SSPI authentication not supported\n"));
			return STATUS_ERROR;

		case AUTH_REQ_CRYPT:
			printfPQExpBuffer(&conn->errorMessage,
					  libpq_gettext("Crypt authentication not supported\n"));
			return STATUS_ERROR;

		case AUTH_REQ_MD5:
		case AUTH_REQ_PASSWORD:
			conn->password_needed = true;
			if (conn->pgpass == NULL || conn->pgpass[0] == '\0')
			{
				printfPQExpBuffer(&conn->errorMessage,
								PQnoPasswordSupplied);
				return STATUS_ERROR;
			}
			if (pg_password_sendauth(conn, conn->pgpass, areq) != STATUS_OK)
			{
				printfPQExpBuffer(&conn->errorMessage,
					 "fe_sendauth: error sending password authentication\n");
				return STATUS_ERROR;
			}
			break;

		case AUTH_REQ_SCM_CREDS:
			if (pg_local_sendauth(conn) != STATUS_OK)
				return STATUS_ERROR;
			break;

		default:
			printfPQExpBuffer(&conn->errorMessage,
			libpq_gettext("authentication method %u not supported\n"), areq);
			return STATUS_ERROR;
	}

	return STATUS_OK;
}


/*
 * pg_fe_getauthname -- returns a pointer to dynamic space containing whatever
 *					 name the user has authenticated to the system
 *
 * if there is an error, return NULL with an error message in errorMessage
 */
char *
pg_fe_getauthname(PQExpBuffer errorMessage)
{
	const char *name = NULL;
	char	   *authn;

#ifdef WIN32
	char		username[128];
	DWORD		namesize = sizeof(username) - 1;
#else
	char		pwdbuf[BUFSIZ];
	struct passwd pwdstr;
	struct passwd *pw = NULL;
#endif

	/*
	 * Some users are using configure --enable-thread-safety-force, so we
	 * might as well do the locking within our library to protect
	 * pqGetpwuid(). In fact, application developers can use getpwuid() in
	 * their application if they use the locking call we provide, or install
	 * their own locking function using PQregisterThreadLock().
	 */
	pglock_thread();

	if (!name)
	{
#ifdef WIN32
		if (GetUserName(username, &namesize))
			name = username;
#else
		if (pqGetpwuid(geteuid(), &pwdstr, pwdbuf, sizeof(pwdbuf), &pw) == 0)
		name = pw->pw_name;
#endif
	}

	authn = name ? strdup(name) : NULL;

	pgunlock_thread();

	return authn;
}


/*
 * PQencryptPassword -- exported routine to encrypt a password
 *
 * This is intended to be used by client applications that wish to send
 * commands like ALTER USER joe PASSWORD 'pwd'.  The password need not
 * be sent in cleartext if it is encrypted on the client side.	This is
 * good because it ensures the cleartext password won't end up in logs,
 * pg_stat displays, etc.  We export the function so that clients won't
 * be dependent on low-level details like whether the enceyption is MD5
 * or something else.
 *
 * Arguments are the cleartext password, and the SQL name of the user it
 * is for.
 *
 * Return value is a malloc'd string, or NULL if out-of-memory.  The client
 * may assume the string doesn't contain any special characters that would
 * require escaping.
 */
char *
PQencryptPassword(const char *passwd, const char *user)
{
	char	   *crypt_pwd;

	crypt_pwd = malloc(MD5_PASSWD_LEN + 1);
	if (!crypt_pwd)
		return NULL;

	if (!pg_md5_encrypt(passwd, user, strlen(user), crypt_pwd))
	{
		free(crypt_pwd);
		return NULL;
	}

	return crypt_pwd;
}
