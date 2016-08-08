    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i
    FROM generate_series(1,10) i;

    -- insert records with null values
    insert into drt_test values(null, null, 1, 2);

    -- Verify null value can be handled
    select message, position from ud_project2(table(
        select a from drt_test where c=1 order by a desc scatter by (a+b)), 
        '/data/hhuang/cdbfast/main/etablefunc_gppc/data/shortMsg.txt') 
    order by position desc;
