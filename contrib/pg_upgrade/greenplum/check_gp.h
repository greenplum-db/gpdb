/*
 *	greenplum/check_gp.h
 *
 *	Portions Copyright (c) 2019-Present, Pivotal Software Inc
 *	contrib/pg_upgrade/greenplum/check_gp.h
 */
void check_heterogeneous_partition(void);

#define CHECK_CHILD_PARTITIONS_DO_NOT_HAVE_DROPPED_COLUMNS \
	"SELECT nsp.nspname AS child_relnamespace, c.relname AS child_relname " \
	"FROM pg_partition_rule pr " \
	"    JOIN pg_attribute a ON a.attrelid = pr.parchildrelid " \
	"    JOIN pg_class c ON c.oid = a.attrelid " \
	"    JOIN pg_namespace nsp ON c.relnamespace = nsp.oid " \
	"WHERE a.attisdropped IS TRUE AND c.relhassubclass IS FALSE;"
