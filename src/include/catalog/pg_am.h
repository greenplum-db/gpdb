/*-------------------------------------------------------------------------
 *
 * pg_am.h
 *	  definition of the system "access method" relation (pg_am)
 *	  along with the relation's initial contents.
 *
 *
 * Portions Copyright (c) 1996-2009, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * $PostgreSQL: pgsql/src/include/catalog/pg_am.h,v 1.53 2008/01/01 19:45:56 momjian Exp $
 *
 * NOTES
 *		the genbki.sh script reads this file and generates .bki
 *		information from the DATA() statements.
 *
 *		XXX do NOT break up DATA() statements into multiple lines!
 *			the scripts are not as smart as you might think...
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_AM_H
#define PG_AM_H

#include "catalog/genbki.h"

/* TIDYCAT_BEGINFAKEDEF

   CREATE TABLE pg_am
   with (camelcase=AccessMethod,  relid=2601)
   (
   amname           name, 
   amstrategies     smallint, 
   amsupport        smallint, 
   amcanorder       boolean, 
   amcanunique      boolean, 
   amcanmulticol    boolean, 
   amoptionalkey    boolean, 
   amindexnulls     boolean, 
   amsearchnulls    boolean, 
   amstorage        boolean, 
   amclusterable    boolean, 
   amcanshrink      boolean, 
   aminsert         regproc, 
   ambeginscan      regproc, 
   amgettuple       regproc, 
   amgetmulti       regproc, 
   amrescan         regproc, 
   amendscan        regproc, 
   ammarkpos        regproc, 
   amrestrpos       regproc, 
   ambuild          regproc, 
   ambulkdelete     regproc, 
   amvacuumcleanup  regproc, 
   amcostestimate   regproc, 
   amoptions        regproc
    );

	create unique index on pg_am(amname) with (indexid=2651, CamelCase=AmName, syscacheid=AMNAME, syscache_nbuckets=4);
	create unique index on pg_am(oid) with (indexid=2652, CamelCase=AmOid, syscacheid=AMOID, syscache_nbuckets=4);

   alter table pg_am add fk aminsert on pg_proc(oid);
   alter table pg_am add fk ambeginscan on pg_proc(oid);
   alter table pg_am add fk amgettuple on pg_proc(oid);
   alter table pg_am add fk amgetmulti on pg_proc(oid);
   alter table pg_am add fk amrescan on pg_proc(oid);
   alter table pg_am add fk amendscan on pg_proc(oid);
   alter table pg_am add fk ammarkpos on pg_proc(oid);
   alter table pg_am add fk amrestrpos on pg_proc(oid);
   alter table pg_am add fk ambuild on pg_proc(oid);
   alter table pg_am add fk ambulkdelete on pg_proc(oid);
   alter table pg_am add fk amvacuumcleanup on pg_proc(oid);
   alter table pg_am add fk amcostestimate on pg_proc(oid);  
   alter table pg_am add fk amoptions on pg_proc(oid);

   TIDYCAT_ENDFAKEDEF
*/



/* ----------------
 *		pg_am definition.  cpp turns this into
 *		typedef struct FormData_pg_am
 * ----------------
 */
#define AccessMethodRelationId	2601

CATALOG(pg_am,2601)
{
	NameData	amname;			/* access method name */
	int2		amstrategies;	/* total number of strategies (operators) by
								 * which we can traverse/search this AM. Zero
								 * if AM does not have a fixed set of strategy
								 * assignments. */
	int2		amsupport;		/* total number of support functions that this
								 * AM uses */
	bool		amcanorder;		/* does AM support ordered scan results? */
	bool		amcanunique;	/* does AM support UNIQUE indexes? */
	bool		amcanmulticol;	/* does AM support multi-column indexes? */
	bool		amoptionalkey;	/* can query omit key for the first column? */
	bool		amindexnulls;	/* does AM support NULL index entries? */
	bool		amsearchnulls;	/* can AM search for NULL index entries? */
	bool		amstorage;		/* can storage type differ from column type? */
	bool		amclusterable;	/* does AM support cluster command? */
	bool		amcanshrink;	/* does AM do anything other than REINDEX in
								 * VACUUM? */
	regproc		aminsert;		/* "insert this tuple" function */
	regproc		ambeginscan;	/* "start new scan" function */
	regproc		amgettuple;		/* "next valid tuple" function */
	regproc		amgetmulti; 	/* "fetch next bitmap" function */ 
	regproc		amrescan;		/* "restart this scan" function */
	regproc		amendscan;		/* "end this scan" function */
	regproc		ammarkpos;		/* "mark current scan position" function */
	regproc		amrestrpos;		/* "restore marked scan position" function */
	regproc		ambuild;		/* "build new index" function */
	regproc		ambulkdelete;	/* bulk-delete function */
	regproc		amvacuumcleanup;	/* post-VACUUM cleanup function */
	regproc		amcostestimate; /* estimate cost of an indexscan */
	regproc		amoptions;		/* parse AM-specific parameters */
} FormData_pg_am;

/* ----------------
 *		Form_pg_am corresponds to a pointer to a tuple with
 *		the format of pg_am relation.
 * ----------------
 */
typedef FormData_pg_am *Form_pg_am;

/* ----------------
 *		compiler constants for pg_am
 * ----------------
 */
#define Natts_pg_am						24
#define Anum_pg_am_amname				1
#define Anum_pg_am_amstrategies			2
#define Anum_pg_am_amsupport			3
#define Anum_pg_am_amcanorder			4
#define Anum_pg_am_amcanunique			5
#define Anum_pg_am_amcanmulticol		6
#define Anum_pg_am_amoptionalkey		7
#define Anum_pg_am_amindexnulls			8
#define Anum_pg_am_amsearchnulls		9
#define Anum_pg_am_amstorage			10
#define Anum_pg_am_amclusterable		11
#define Anum_pg_am_amcanshrink			12
#define Anum_pg_am_aminsert				13
#define Anum_pg_am_ambeginscan			14
#define Anum_pg_am_amgettuple			15
#define Anum_pg_am_amgetmulti			16
#define Anum_pg_am_amrescan				17
#define Anum_pg_am_amendscan			18
#define Anum_pg_am_ammarkpos			19
#define Anum_pg_am_amrestrpos			20
#define Anum_pg_am_ambuild				21
#define Anum_pg_am_ambulkdelete			22
#define Anum_pg_am_amvacuumcleanup		23
#define Anum_pg_am_amcostestimate		24
#define Anum_pg_am_amoptions			25

/* ----------------
 *		initial contents of pg_am
 * ----------------
 */

DATA(insert OID = 403 (  btree	5 1 t t t t t t f t t btinsert btbeginscan btgettuple btgetmulti btrescan btendscan btmarkpos btrestrpos btbuild btbulkdelete btvacuumcleanup btcostestimate btoptions ));
DESCR("b-tree index access method");
#define BTREE_AM_OID 403
DATA(insert OID = 405 (  hash	1 1 f f f f f f f f t hashinsert hashbeginscan hashgettuple hashgetmulti hashrescan hashendscan hashmarkpos hashrestrpos hashbuild hashbulkdelete hashvacuumcleanup hashcostestimate hashoptions ));
DESCR("hash index access method");
#define HASH_AM_OID 405
DATA(insert OID = 783 (  gist	0 7 f f t t t t t t t gistinsert gistbeginscan gistgettuple gistgetmulti gistrescan gistendscan gistmarkpos gistrestrpos gistbuild gistbulkdelete gistvacuumcleanup gistcostestimate gistoptions ));
DESCR("GiST index access method");
#define GIST_AM_OID 783
DATA(insert OID = 2742 (  gin	0 4 f f f f f f t f t gininsert ginbeginscan gingettuple gingetmulti ginrescan ginendscan ginmarkpos ginrestrpos ginbuild ginbulkdelete ginvacuumcleanup gincostestimate ginoptions ));
DESCR("GIN index access method");
#define GIN_AM_OID 2742
DATA(insert OID = 3013 (  bitmap	5 1 f f t t t f f f f bminsert bmbeginscan bmgettuple bmgetmulti bmrescan bmendscan bmmarkpos bmrestrpos bmbuild bmbulkdelete bmvacuumcleanup bmcostestimate bmoptions ));
DESCR("bitmap index access method");
#define BITMAP_AM_OID 3013

/*
 * Am_btree AM values for FormData_pg_am.
 */
#define Am_btree \
	{"btree"}, 5, 1, true, true, true, true, true, true, false, true, true, BTINSERT_OID, BTBEGINSCAN_OID, BTGETTUPLE_OID, BTGETMULTI_OID, BTRESCAN_OID, BTENDSCAN_OID, BTMARKPOS_OID, BTRESTRPOS_OID, BTBUILD_OID, BTBULKDELETE_OID, BTVACUUMCLEANUP_OID, BTCOSTESTIMATE_OID, BTOPTIONS_OID


#endif   /* PG_AM_H */
