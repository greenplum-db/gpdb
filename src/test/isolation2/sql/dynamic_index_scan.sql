CREATE EXTENSION IF NOT EXISTS gp_inject_fault;

-- Purpose of this test is to check that dynamic index scan works on a
-- partitioned table with multiple multi-column indexes
CREATE TABLE a_table_with_multi_column_index (
    id character varying(64),
    data character varying(50),
    partition_key int
) DISTRIBUTED BY (partition_key) PARTITION BY RANGE(partition_key)
(START(0) END(10) EVERY(1));
CREATE INDEX a_multi_column_index ON a_table_with_multi_column_index USING btree (data, id);
CREATE INDEX another_multi_column_index ON a_table_with_multi_column_index USING btree (data, partition_key);

INSERT INTO a_table_with_multi_column_index SELECT i||'id', 'some data', i%10 FROM generate_series(1, 100)i;

-- Following fault causes an extra allocation during dynamic index scan. The
-- purpose of that is to prevent the allocator from hiding a stale pointer
-- where it would reuse the stale address and populate it with valid data.
SELECT gp_inject_fault('dynamic_index_scan_context_reset', 'skip', dbid) FROM pg_catalog.gp_segment_configuration WHERE role = 'p';

SELECT count(b.id)
FROM a_table_with_multi_column_index a, a_table_with_multi_column_index b
WHERE a.id = b.id
AND Upper(a.id) LIKE '%ID'
AND a.data = 'some data';

SELECT gp_inject_fault('dynamic_index_scan_context_reset', 'status', dbid) FROM pg_catalog.gp_segment_configuration WHERE role = 'p';

SELECT gp_inject_fault('dynamic_index_scan_context_reset', 'reset', dbid) FROM pg_catalog.gp_segment_configuration WHERE role = 'p';
