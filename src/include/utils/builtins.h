/*-------------------------------------------------------------------------
 *
 * builtins.h
 *	  Declarations for operations on built-in types.
 *
 *
 * Portions Copyright (c) 2005-2010, Greenplum inc
 * Portions Copyright (c) 2012-Present VMware, Inc. or its affiliates.
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * src/include/utils/builtins.h
 *
 *-------------------------------------------------------------------------
 */
#ifndef BUILTINS_H
#define BUILTINS_H

#include "fmgr.h"
#include "nodes/nodes.h"
#include "utils/fmgrprotos.h"

/* Sign + the most decimal digits an 8-byte number could have */
#define MAXINT8LEN 20

/* bool.c */
extern bool parse_bool(const char *value, bool *result);
extern bool parse_bool_with_len(const char *value, size_t len, bool *result);

/* domains.c */
extern void domain_check(Datum value, bool isnull, Oid domainType,
						 void **extra, MemoryContext mcxt);
extern int	errdatatype(Oid datatypeOid);
extern int	errdomainconstraint(Oid datatypeOid, const char *conname);

/* encode.c */
extern unsigned hex_encode(const char *src, unsigned len, char *dst);
extern unsigned hex_decode(const char *src, unsigned len, char *dst);

/* int.c */
extern int2vector *buildint2vector(const int16 *int2s, int n);

/* name.c */
extern int	namecpy(Name n1, const NameData *n2);
extern int	namestrcpy(Name name, const char *str);
extern int	namestrcmp(Name name, const char *str);

/* numutils.c */
extern int32 pg_atoi(const char *s, int size, int c);
extern int16 pg_strtoint16(const char *s);
extern int32 pg_strtoint32(const char *s);
extern void pg_itoa(int16 i, char *a);
int			pg_ultoa_n(uint32 l, char *a);
int			pg_ulltoa_n(uint64 l, char *a);
extern void pg_ltoa(int32 l, char *a);
extern void pg_lltoa(int64 ll, char *a);
extern char *pg_ultostr_zeropad(char *str, uint32 value, int32 minwidth);
extern char *pg_ultostr(char *str, uint32 value);
extern uint64 pg_strtouint64(const char *str, char **endptr, int base);

/* dbsize.c */
extern int64 get_size_from_segDBs(const char *cmd);

/* oid.c */
extern oidvector *buildoidvector(const Oid *oids, int n);
extern Oid	oidparse(Node *node);

/* pseudotypes.c */
extern int	oid_cmp(const void *p1, const void *p2);

/* regexp.c */
extern char *regexp_fixed_prefix(text *text_re, bool case_insensitive,
								 Oid collation, bool *exact);

/* ruleutils.c */
extern bool quote_all_identifiers;
extern char *pg_get_constraintexpr_string(Oid constraintId);

extern const char *quote_identifier(const char *ident);
extern char *quote_qualified_identifier(const char *qualifier,
										const char *ident);
extern void generate_operator_clause(fmStringInfo buf,
									 const char *leftop, Oid leftoptype,
									 Oid opoid,
									 const char *rightop, Oid rightoptype);

/* varchar.c */
extern int	bpchartruelen(char *s, int len);

/* popular functions from varlena.c */
extern text *cstring_to_text(const char *s);
extern text *cstring_to_text_with_len(const char *s, int len);
extern char *text_to_cstring(const text *t);
extern void text_to_cstring_buffer(const text *src, char *dst, size_t dst_len);

#define CStringGetTextDatum(s) PointerGetDatum(cstring_to_text(s))
#define TextDatumGetCString(d) text_to_cstring((text *) DatumGetPointer(d))

/* xid.c */
extern int	xidComparator(const void *arg1, const void *arg2);
extern int	xidLogicalComparator(const void *arg1, const void *arg2);

/* inet_cidr_ntop.c */
extern char *inet_cidr_ntop(int af, const void *src, int bits,
							char *dst, size_t size);

/* inet_net_pton.c */
extern int	inet_net_pton(int af, const char *src,
						  void *dst, size_t size);

/* network.c */
extern double convert_network_to_scalar(Datum value, Oid typid, bool *failure);
extern Datum network_scan_first(Datum in);
extern Datum network_scan_last(Datum in);
extern void clean_ipv6_addr(int addr_family, char *addr);

/* numeric.c */
extern Datum numeric_float8_no_overflow(PG_FUNCTION_ARGS);

/* format_type.c */

/* Control flags for format_type_extended */
#define FORMAT_TYPE_TYPEMOD_GIVEN	0x01	/* typemod defined by caller */
#define FORMAT_TYPE_ALLOW_INVALID	0x02	/* allow invalid types */
#define FORMAT_TYPE_FORCE_QUALIFY	0x04	/* force qualification of type */
extern char *format_type_extended(Oid type_oid, int32 typemod, bits16 flags);

extern char *format_type_be(Oid type_oid);
extern char *format_type_be_qualified(Oid type_oid);
extern char *format_type_with_typemod(Oid type_oid, int32 typemod);

extern int32 type_maximum_size(Oid type_oid, int32 typemod);

/* quote.c */
extern char *quote_literal_cstr(const char *rawstr);

/* query_metrics.c */
extern Datum gp_instrument_shmem_summary(PG_FUNCTION_ARGS);

/* utils/gp/segadmin.c */
extern bool gp_activate_standby(void);

/* utils/adt/genfile.c */
extern Datum pg_file_write(PG_FUNCTION_ARGS);
extern Datum pg_file_rename(PG_FUNCTION_ARGS);
extern Datum pg_file_unlink(PG_FUNCTION_ARGS);
extern Datum pg_logdir_ls(PG_FUNCTION_ARGS);

#endif							/* BUILTINS_H */
