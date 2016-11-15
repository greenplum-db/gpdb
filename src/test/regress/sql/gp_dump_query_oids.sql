-- Test the built-in gp_dump_query function.
SELECT gp_dump_query_oids('SELECT 123');
SELECT gp_dump_query_oids('SELECT * FROM pg_proc');
SELECT gp_dump_query_oids('SELECT length(proname) FROM pg_proc');

-- with EXPLAIN
SELECT gp_dump_query_oids('explain SELECT length(proname) FROM pg_proc');

-- with a multi-query statement
SELECT gp_dump_query_oids('SELECT length(proname) FROM pg_proc; SELECT abs(relpages) FROM pg_class');
