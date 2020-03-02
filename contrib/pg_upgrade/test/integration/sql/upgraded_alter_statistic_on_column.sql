SELECT count(*) FROM explicitly_set_statistic_table;
SELECT attname, attstattarget from pg_attribute, pg_class where attrelid=oid and relname='explicitly_set_statistic_table' and attname='col1';
