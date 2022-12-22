/*-------------------------------------------------------------------------
 *
 * pg_statistic_ext_d.h
 *    Macro definitions for pg_statistic_ext
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * NOTES
 *  ******************************
 *  *** DO NOT EDIT THIS FILE! ***
 *  ******************************
 *
 *  It has been GENERATED by src/backend/catalog/genbki.pl
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_STATISTIC_EXT_D_H
#define PG_STATISTIC_EXT_D_H

#define StatisticExtRelationId 3381

#define Anum_pg_statistic_ext_oid 1
#define Anum_pg_statistic_ext_stxrelid 2
#define Anum_pg_statistic_ext_stxname 3
#define Anum_pg_statistic_ext_stxnamespace 4
#define Anum_pg_statistic_ext_stxowner 5
#define Anum_pg_statistic_ext_stxkeys 6
#define Anum_pg_statistic_ext_stxkind 7

#define Natts_pg_statistic_ext 7


#define STATS_EXT_NDISTINCT			'd'
#define STATS_EXT_DEPENDENCIES		'f'
#define STATS_EXT_MCV				'm'


#endif							/* PG_STATISTIC_EXT_D_H */
