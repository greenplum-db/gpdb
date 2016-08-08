-- start_ignore
create language plpythonu;

create or replace function find_operator(query text, operator_name text) returns text as
$$
rv = plpy.execute(query)
search_text = operator_name
result = ['false']
for i in range(len(rv)):
    cur_line = rv[i]['QUERY PLAN']
    if search_text.lower() in cur_line.lower():
        result = ['true']
        break
return result
$$
language plpythonu;
-- end_ignore

-- ETF sub-query contains join table / view
-- Table t1, t2 are distributed by column a
-- Table t3, t5 are distributed by columns a and e
-- Table t4, t6 are distributed randomly
    DROP TABLE IF EXISTS t2;
    DROP TABLE IF EXISTS t5;
    DROP TABLE IF EXISTS t6;
    DROP TABLE IF EXISTS t4;
    DROP TABLE IF EXISTS t3;

    CREATE TABLE t3 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a, e);

    CREATE TABLE t2 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a);

    INSERT INTO t2 SELECT i, i/3, i%2, 100-i, 'text'||i
    FROM generate_series(1,100) i;

    DROP TABLE IF EXISTS t4;
    CREATE TABLE t4 (a int, b int, c int, d int, e text)
    DISTRIBUTED RANDOMLY;

    CREATE TABLE t5 (a int, b int, c int, d int, e text)
    DISTRIBUTED BY (a, e);

    INSERT INTO t5 SELECT i, i/3, i%2, 100-i, 'text'||i
    FROM generate_series(1,100) i;

    CREATE TABLE t6 (a int, b int, c int, d int, e text)
    DISTRIBUTED RANDOMLY;

    INSERT INTO t6 SELECT i, i/3, i%2, 100-i, 'text'||i
    FROM generate_series(1,100) i;

    -- Join t1 and t2 that are both distributed by single column a
    -- SCATTER BY the same single distribution key a
    -- of the source tables t1 and t2
    -- No redistribution should be involved
    SELECT find_operator('EXPLAIN SELECT * FROM transform( TABLE(select t1.a, t2.e from t1 join t2 on t1.a = t2.a scatter by t1.a) );','Redistribute Motion');

    -- Join t3 and t5 that are both distributed by composite key columns a, e
    -- SCATTER BY the same composite key a, e
    -- of the source tables t3 and t5
    -- No redistribution should be involved
    SELECT find_operator('EXPLAIN SELECT * FROM transform( TABLE(select t3.a, t5.e from t3 join t5 on (t3.a = t5.a and t3.e = t5.e) scatter by t3.a, t3.e) );','Redistribute Motion');

    -- Both source tables are DISTRIBUTED RANDOMLY
    -- Redistribution is needed
    SELECT find_operator('EXPLAIN SELECT * FROM transform( TABLE(select t4.a, t6.e from t4 join t6 on (t4.a = t6.a and t4.e = t6.e) scatter by t4.a, t6.e) );','Redistribute Motion');

-- start_ignore
drop function if exists find_operator(query text, operator_name text);
-- end_ignore
