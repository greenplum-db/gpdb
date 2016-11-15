-- Test the built-in gp_dump_query function.

SELECT gp_dump_query_oids('SELECT 123');
SELECT gp_dump_query_oids('SELECT * from pg_proc');
SELECT gp_dump_query_oids('SELECT length(proname) from pg_proc');
SELECT gp_dump_query_oids('explain SELECT length(proname) from pg_proc');
