/*
 *	greenplum/check_gp.h
 *
 *	Portions Copyright (c) 2019-Present, Pivotal Software Inc
 *	contrib/pg_upgrade/greenplum/check_gp.h
 */
void check_heterogeneous_partition(void);

/*
 * This query gathers all the root (rp) and child (cp1 and cp2) partition
 * information necessary to detect if a partition table is a heterogeneous
 * partition table. The WHERE filter compares the root and child partition
 * information to exclude out the valid homogeneous partition table
 * definitions (as documented in the check_heterogeneous_partition() header).
 *
 * Note: We ignore subroot partitions because they also do not have data like
 * the root partition. We also defer partition tables where the root partition
 * and all of its child partitions have the same dropped column reference but
 * at least one child partition has extra dropped column references (the
 * CHECK_PARTITION_TABLE_MATCHES_DROPPED_COLUMN_ATTRIBUTES query will catch
 * these special problematic child partitions).
 */
#define CHECK_PARTITION_TABLE_DROPPED_COLUMN_REFERENCES \
	"SELECT cp1.childnamespace, cp1.childrelname " \
	"FROM ( " \
	"        SELECT p.parrelid, rule.parchildrelid, n.nspname AS childnamespace, c.relname AS childrelname, c.relnatts AS childnatts, " \
	"               sum(CASE WHEN a.attisdropped THEN 1 ELSE 0 END) AS childnumattisdropped " \
	"        FROM pg_catalog.pg_partition p " \
	"            JOIN pg_catalog.pg_partition_rule rule ON p.oid=rule.paroid AND NOT p.paristemplate " \
	"            JOIN pg_catalog.pg_class c ON rule.parchildrelid = c.oid AND NOT c.relhassubclass " \
	"            JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace " \
	"            JOIN pg_catalog.pg_attribute a ON rule.parchildrelid = a.attrelid AND a.attnum > 0 " \
	"        GROUP BY p.parrelid, rule.parchildrelid, n.nspname, c.relname, c.relnatts " \
	"    ) cp1 " \
	"    JOIN ( " \
	"        SELECT p.parrelid, min(c.relnatts) AS minchildnatts, max(c.relnatts) AS maxchildnatts " \
	"        FROM pg_catalog.pg_partition p " \
	"            JOIN pg_catalog.pg_partition_rule rule ON p.oid=rule.paroid AND NOT p.paristemplate " \
	"            JOIN pg_catalog.pg_class c ON rule.parchildrelid = c.oid AND NOT c.relhassubclass " \
	"        GROUP BY p.parrelid " \
	"    ) cp2 ON cp2.parrelid = cp1.parrelid " \
	"    JOIN ( " \
	"        SELECT c.oid, n.nspname AS parnamespace, c.relname AS parrelname, c.relnatts AS parnatts, " \
	"               sum(CASE WHEN a.attisdropped THEN 1 ELSE 0 END) AS parnumattisdropped " \
	"        FROM pg_catalog.pg_partition p " \
	"            JOIN pg_catalog.pg_class c ON p.parrelid = c.oid AND NOT p.paristemplate AND p.parlevel = 0 " \
	"            JOIN pg_catalog.pg_namespace n ON n.oid = c.relnamespace " \
	"            JOIN pg_catalog.pg_attribute a ON c.oid = a.attrelid AND a.attnum > 0 " \
	"        GROUP BY c.oid, n.nspname, c.relname, c.relnatts " \
	"    ) rp ON rp.oid = cp1.parrelid " \
	"WHERE NOT (rp.parnumattisdropped = 0 AND rp.parnatts = cp1.childnatts) AND " \
	"      NOT (rp.parnumattisdropped > 0 AND cp2.minchildnatts = cp2.maxchildnatts AND " \
	"           (rp.parnatts = cp1.childnatts OR cp1.childnumattisdropped = 0)) AND " \
	"      NOT (rp.parnumattisdropped > 0 AND cp2.minchildnatts != cp2.maxchildnatts AND " \
	"           cp2.minchildnatts < rp.parnatts AND cp1.childnumattisdropped = 0) AND " \
	"      NOT (rp.parnumattisdropped > 0 AND cp2.minchildnatts != cp2.maxchildnatts AND " \
	"           cp2.minchildnatts >= rp.parnatts) " \
	"ORDER BY rp.oid, cp1.parchildrelid;"

/*
 * This query gathers all child partition dropped column attributes and
 * compares them to their respective root partition's attribute list to detect
 * any mismatched attribute names, dropped column references, types, lengths,
 * and alignments. The comparison is done in attnum order.
 *
 * Note: Column name mismatches can occur due to changes in the relative order
 * of dropped columns. For instance consider the two tables root(a, b,
 * ...dropped...) and child(a, ...dropped..., b). In the query below, we
 * exclude partition tables where the root partition does not have dropped
 * column references or only the root partition has dropped column
 * references. We also exclude subroot partitions because they also do not
 * have data like the root partition.
 */
#define CHECK_PARTITION_TABLE_MATCHES_DROPPED_COLUMN_ATTRIBUTES \
	"WITH root_dropped_attr AS ( " \
	"    SELECT par.oid AS paroid, a.attnum, a.attisdropped, a.attname, a.attlen, a.atttypid, a.attalign " \
	"    FROM pg_catalog.pg_partition par " \
	"        JOIN pg_catalog.pg_attribute a ON a.attrelid = par.parrelid " \
	"    WHERE NOT par.paristemplate AND a.attisdropped) " \
	"SELECT DISTINCT child_dropped_attr.parchildrelid::regclass " \
	"FROM root_dropped_attr " \
	"    RIGHT JOIN ( " \
	"        SELECT pr.paroid, pr.parchildrelid, a.attnum, a.attisdropped, a.attname, a.attlen, a.atttypid, a.attalign " \
	"        FROM pg_catalog.pg_partition_rule pr " \
	"            JOIN pg_catalog.pg_class c ON c.oid = pr.parchildrelid AND NOT c.relhassubclass " \
	"            JOIN pg_catalog.pg_attribute a ON a.attrelid = pr.parchildrelid " \
	"        WHERE a.attisdropped AND pr.paroid IN (SELECT DISTINCT paroid FROM root_dropped_attr) " \
	"    ) child_dropped_attr ON child_dropped_attr.paroid = root_dropped_attr.paroid " \
	"                            AND child_dropped_attr.attnum = root_dropped_attr.attnum " \
	"WHERE root_dropped_attr.attisdropped IS DISTINCT FROM child_dropped_attr.attisdropped " \
	"      OR root_dropped_attr.attname IS DISTINCT FROM child_dropped_attr.attname " \
	"      OR root_dropped_attr.attlen IS DISTINCT FROM child_dropped_attr.attlen " \
	"      OR root_dropped_attr.atttypid IS DISTINCT FROM child_dropped_attr.atttypid " \
	"      OR root_dropped_attr.attalign IS DISTINCT FROM child_dropped_attr.attalign;"
