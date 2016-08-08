    -- Create Dynamic Return Type test table drt_test
    DROP TABLE IF EXISTS drt_test;
    CREATE TABLE drt_test (a int, b int, c int, d int)
    DISTRIBUTED BY (a);
    --
    -- Load drt_test table with 10 records
    --
    INSERT INTO drt_test SELECT i, i/3, i%2, 10-i 
    FROM generate_series(1,10) i;

    select * from drt_test order by a;

