SELECT count(*) FROM gp_segment_configuration WHERE preferred_role = 'm';
SELECT * FROM pg_stat_replication;

--\! gpinitstandby --flags

SELECT count(*) FROM gp_segment_configuration WHERE preferred_role = 'm';
SELECT * FROM pg_stat_replication;
SELECT * FROM pg_stat_replication_segments;

\! ./gpsegwalrep.py init 
\! ./gpsegwalrep.py start

SELECT count(*) FROM gp_segment_configuration WHERE preferred_role = 'm';
SELECT * FROM pg_stat_replication;
SELECT * FROM pg_stat_replication_segments;
SELECT * FROM pg_stat_replication_all;

--CREATE TABLE replication_views_table (a int);
--INSERT INTO replication_views_table values(1);
--CHECKPOINT;

--\! ./gpsegwalrep.py destroy

--SELECT count(*) FROM gp_segment_configuration WHERE preferred_role = 'm';
--SELECT * FROM pg_stat_replication;
--SELECT * FROM pg_stat_replication_segments;
--SELECT * FROM pg_stat_replication_all;
