    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- The second pair example (ud_describe2 and ud_project2) read text data
    -- specified as 2nd argument to project function

    -- create describe (callback) function with User Context
    -- both input and output type must be internal
    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/tabfunc_gppc_demo', 'userdata_describe2'
      LANGUAGE C;

    -- create dynamic return type (drt) table function
    -- using the callback function with user context
    -- the return type must be defined as set of record
    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/tabfunc_gppc_demo', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- When sub query has dup rows
    select * from ud_project2(table(
        select b from drt_test order by a scatter by (a)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;
