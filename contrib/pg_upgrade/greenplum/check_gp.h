void check_heterogeneous_partition(void);

#define CHECK_PARTITION_TABLE_MATCHES_COLUMN_COUNT \
	"SELECT parrelid, c1.relnatts, minchildnatts, maxchildnatts " \
	"FROM ( " \
	"    SELECT parrelid::regclass, min(c2.relnatts) minchildnatts, max(c2.relnatts) maxchildnatts " \
	"    FROM pg_partition par " \
	"    JOIN pg_partition_rule rule ON par.oid=rule.paroid AND NOT par.paristemplate " \
	"    JOIN pg_class c2 ON parchildrelid = c2.oid GROUP BY parrelid " \
	") t JOIN pg_class c1 ON c1.oid = parrelid WHERE c1.relnatts!=minchildnatts OR c1.relnatts!=maxchildnatts;"

#define CHECK_PARTITION_TABLE_MATCHES_COLUMN_ATTRIBUTES \
	"SELECT parrelid::regclass, att1.attnum, rule.parchildrelid::regclass, att1.attname attname1, att2.attname attname2, att1.attisdropped attisdropped1, att2.attisdropped attisdropped2, att1.attlen attlen1, att2.attlen attlen2, att1.atttypid atttypid1, att2.atttypid atttypid2, att1.attalign attalign1, att2.attalign attalign2 " \
	"FROM pg_partition par join pg_partition_rule rule on par.oid=rule.paroid and not par.paristemplate  " \
	"JOIN pg_attribute att1 ON att1.attrelid = par.parrelid " \
	"JOIN pg_attribute att2 ON att2.attrelid = rule.parchildrelid AND att1.attnum = att2.attnum AND att1.attnum > 0 " \
				"  AND NOT (att1.attisdropped = att2.attisdropped AND att1.attname = att2.attname AND att1.attlen = att2.attlen AND att1.atttypid = att2.atttypid AND att1.attalign = att2.attalign);"
