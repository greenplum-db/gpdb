    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    CREATE OR REPLACE FUNCTION ud_describe(internal) RETURNS internal
      AS '$libdir/tabfunc_gppc_demo', 'userdata_describe'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION ud_project1(anytable) RETURNS setof RECORD
      AS '$libdir/tabfunc_gppc_demo', 'userdata_project'
      LANGUAGE C
      WITH (describe = ud_describe);

    -- Simple check of DRT_UC_ETF
    select * from ud_project1( table(select * from drt_test) );

    CREATE OR REPLACE FUNCTION ud_describe2(internal) RETURNS internal
      AS '$libdir/tabfunc_gppc_demo', 'userdata_describe2'
      LANGUAGE C;

    CREATE OR REPLACE FUNCTION ud_project2(anytable, text) RETURNS setof RECORD
      AS '$libdir/tabfunc_gppc_demo', 'userdata_project2'
      LANGUAGE C
      WITH (describe = ud_describe2);

    -- Simple check of DRT_UC_ETF
    select * from ud_project2(table(
        select a from drt_test order by a scatter by (a)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position;

    -- Check describe (callback) functions are registerred in pg_proc_callback
    select * from pg_proc_callback 
    where ((profnoid='ud_project1'::regproc and procallback='ud_describe'::regproc)
    or (profnoid='ud_project2'::regproc and procallback='ud_describe2'::regproc)); 

