drop table test_co;

CREATE TABLE test_co_01 (a int, b int, c int, d int, e int) with (appendonly=true, orientation=column);
CREATE TABLE test_co_02 (a int, b int, c int, d int, e int) with (appendonly=true, orientation=column);
