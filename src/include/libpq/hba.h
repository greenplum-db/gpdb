/*-------------------------------------------------------------------------
 *
 * hba.h
 *	  Interface to hba.c
 *
 *
 * $PostgreSQL: pgsql/src/include/libpq/hba.h,v 1.56 2009/06/11 14:49:11 momjian Exp $
 *
 *-------------------------------------------------------------------------
 */
#ifndef HBA_H
#define HBA_H

#include "nodes/pg_list.h"
#include "libpq/pqcomm.h"


typedef enum UserAuth
{
	uaReject,
	uaImplicitReject,
	uaKrb5,
	uaTrust,
	uaIdent,
	uaPassword,
	uaMD5,
	uaGSS,
	uaSSPI,
	uaPAM,
	uaLDAP,
	uaCert,
	uaRADIUS
} UserAuth;

typedef enum IPCompareMethod
{
	ipCmpMask,
	ipCmpSameHost,
	ipCmpSameNet
} IPCompareMethod;

typedef enum ConnType
{
	ctLocal,
	ctHost,
	ctHostSSL,
	ctHostNoSSL
} ConnType;

typedef struct
{
	int			linenumber;
	ConnType	conntype;
	char	   *database;
	char	   *role;
	struct sockaddr_storage addr;
	struct sockaddr_storage mask;
	IPCompareMethod ip_cmp_method;
	UserAuth	auth_method;

	char	   *usermap;
	char	   *pamservice;
	bool		ldaptls;
	char	   *ldapserver;
	int			ldapport;
	char	   *ldapbinddn;
	char	   *ldapbindpasswd;
	char	   *ldapsearchattribute;
	char	   *ldapbasedn;
	char	   *ldapprefix;
	char	   *ldapsuffix;
	bool		clientcert;
	char	   *krb_server_hostname;
	char	   *krb_realm;
	bool		include_realm;
	char	   *radiusserver;
	char	   *radiussecret;
	char	   *radiusidentifier;
	int			radiusport;
} HbaLine;

/* kluge to avoid including libpq/libpq-be.h here */
typedef struct Port hbaPort;

extern List **get_role_line(const char *role);
extern List *get_role_intervals(const char *role);
extern bool load_hba(void);
extern void load_ident(void);
extern void load_role(void);
extern void load_role_interval(void);
extern void force_load_role(void);
extern int	hba_getauthmethod(hbaPort *port);
extern bool read_pg_database_line(FILE *fp, char *dbname, Oid *dboid,
					  Oid *dbtablespace, TransactionId *dbfrozenxid);
extern int check_usermap(const char *usermap_name,
			  const char *pg_role, const char *auth_user,
			  bool case_sensitive);
extern bool check_same_host_or_net(SockAddr *raddr, IPCompareMethod method);
extern bool pg_isblank(const char c);

#endif   /* HBA_H */
