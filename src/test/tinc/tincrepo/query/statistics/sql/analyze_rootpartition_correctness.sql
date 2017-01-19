-- @author raghav
-- @created 2014-06-27 12:00:00
-- @product_version gpdb: [4.3-],hawq: [1.2.1.0-]
-- @db_name analyze_db
-- @tags analyze ORCA
-- @optimizer_mode both
-- @description Test 'ANALYZE ROOTPARTITION' correctness

-- check if analyze and analyze rootpartition generates same statistics
analyze arp_test3;

select schemaname, tablename, attname, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds from pg_stats where tablename like 'arp_test3%' order by tablename, attname;

set allow_system_table_mods="DML";

delete from pg_statistic where starelid='arp_test3'::regclass;

select schemaname, tablename, attname, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds from pg_stats where tablename like 'arp_test3%' order by tablename, attname;

analyze rootpartition arp_test3;

select schemaname, tablename, attname, null_frac, avg_width, n_distinct, most_common_vals, most_common_freqs, histogram_bounds from pg_stats where tablename like 'arp_test3%' order by tablename, attname;

drop table arp_test3;

