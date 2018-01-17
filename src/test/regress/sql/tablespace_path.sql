select gp_tablespace_path('/tmp/tbl', 1) ~ '^/tmp/tbl/GPDB_([0-9]+\.)([0-9]+)_([0-9]+)_db1$' as path;
select gp_tablespace_path(repeat('a', 3000)::text, 1);
