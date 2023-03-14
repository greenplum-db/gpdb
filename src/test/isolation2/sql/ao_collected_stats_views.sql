-- @Description Tests collected statistics reporting for append-optimized tables

-- Helper function that ensures stats collector receives stat from the latest operation.
create or replace function wait_until_heap_blks_read_change_from(table_name text, stat_val_old int)
    returns text as $$
declare
    stat_val int; /* in func */
    i int; /* in func */
begin
    i := 0; /* in func */
    while i < 1200 loop
        select heap_blks_read into stat_val from pg_statio_all_tables where relname = table_name; /* in func */
        if stat_val != stat_val_old then /* in func */
            return 'OK'; /* in func */
        end if; /* in func */
        perform pg_sleep(0.1); /* in func */
        perform pg_stat_clear_snapshot(); /* in func */
        i := i + 1; /* in func */
    end loop; /* in func */
    return 'Fail'; /* in func */
end; /* in func */
$$ language plpgsql;

-- Setup test table
DROP TABLE IF EXISTS ao_statio;
DROP TABLE IF EXISTS helper_table;
CREATE TABLE ao_statio(a int, b int) USING ao_row;
INSERT INTO ao_statio SELECT 0, i FROM generate_series(1, 100000)i;

CREATE TABLE helper_table AS SELECT 0::int AS seg1, (pg_relation_size('ao_statio') + (current_setting('block_size')::int - 1)) / current_setting('block_size')::int AS heap_blks_total;

-- heap_blks_read should remain zero after INSERT
1U: SELECT heap_blks_read, heap_blks_hit FROM pg_statio_all_tables WHERE relname = 'ao_statio';

-- Perform seq scan and wait until the stats collector receives the stats update
1U&: SELECT wait_until_heap_blks_read_change_from('ao_statio', 0);
SELECT sum(a+b) FROM ao_statio;
1U<:

-- heap_blks_read should equal to heap_blks_total
1U: SELECT heap_blks_read = (SELECT heap_blks_total FROM helper_table) AS heap_blks_read_all, heap_blks_hit FROM pg_statio_all_tables WHERE relname = 'ao_statio';

-- cleanup