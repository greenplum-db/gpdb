CREATE TABLE table_used_in_sum_plus_42(a int);
INSERT INTO table_used_in_sum_plus_42 SELECT 1 FROM generate_series(1,10);

SELECT sum_plus_42(a) FROM table_used_in_sum_plus_42;

SELECT attrelid::regclass, array_accum(attname)
    FROM pg_attribute
    WHERE attnum > 0 AND attrelid = 'pg_tablespace'::regclass
    GROUP BY attrelid;

SELECT attrelid::regclass, array_accum(atttypid::regtype)
    FROM pg_attribute
    WHERE attnum > 0 AND attrelid = 'pg_tablespace'::regclass
    GROUP BY attrelid;
