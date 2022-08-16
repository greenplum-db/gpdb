CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

0:CREATE TABLE a_partition_table_for_analyze_cancellation (
    a_date date NOT NULL,
    a_bigint bigint NOT NULL,
    b_bigint bigint NOT NULL
)
WITH (appendonly='true',  orientation='column')
DISTRIBUTED BY (a_bigint, b_bigint)
PARTITION BY RANGE(a_date)
    (
    PARTITION p1 START ('2018-01-01'::date) END ('2018-12-31'::date) WITH (appendonly='true', orientation='column')
              COLUMN a_date ENCODING (compresstype=zlib)
              COLUMN a_bigint ENCODING (compresstype=zlib)
              COLUMN b_bigint ENCODING (compresstype=zlib),
    PARTITION p2 START ('2019-01-01'::date) END ('2019-12-31'::date) WITH (appendonly='true', orientation='column')
              COLUMN a_date ENCODING (compresstype=zlib)
              COLUMN a_bigint ENCODING (compresstype=zlib)
              COLUMN b_bigint ENCODING (compresstype=zlib),
    PARTITION p3 START ('2020-01-01'::date) END ('2020-12-31'::date) WITH (appendonly='true', orientation='column')
              COLUMN a_date ENCODING (compresstype=zlib)
              COLUMN a_bigint ENCODING (compresstype=zlib)
              COLUMN b_bigint ENCODING (compresstype=zlib)
    );
0:INSERT INTO a_partition_table_for_analyze_cancellation VALUES(timestamp '2018-01-01 10:00:00', 1, 3);
0:INSERT INTO a_partition_table_for_analyze_cancellation VALUES(timestamp '2019-01-01 12:00:00', 2, 4);
0:INSERT INTO a_partition_table_for_analyze_cancellation VALUES(timestamp '2020-01-01 13:00:00', 3, 5);

0: SELECT gp_inject_fault('zlib_decompress_after_decompress_fn', 'sleep', '', '', '', 1, -1, 3600, dbid) FROM gp_segment_configuration WHERE content=1 AND role='p';
0&: SELECT gp_wait_until_triggered_fault('zlib_decompress_after_decompress_fn', 1, dbid) FROM gp_segment_configuration WHERE content=1 AND role='p';

-- ANALYZE on AO/CO table with zlib compression will hit and fault injection
-- 'zlib_decompress_after_decompress_fn'
1&: ANALYZE a_partition_table_for_analyze_cancellation_1_prt_p3;

-- It should still be possible to cancel the ANALYZE backend
2: SELECT pg_cancel_backend(pid) FROM pg_stat_activity WHERE pid <> pg_backend_pid() AND application_name='pg_regress';
0<:
1<:

SELECT gp_inject_fault('zlib_decompress_after_decompress_fn', 'reset', dbid) FROM gp_segment_configuration;
