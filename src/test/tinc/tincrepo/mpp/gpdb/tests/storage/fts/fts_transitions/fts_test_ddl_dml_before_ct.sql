drop table test_co;

CREATE TABLE test_co (a int, b int, c int, d int, e int) with (appendonly=true, orientation=column);
CREATE TABLE test_ao (a int, b int) with (appendonly=true);
CREATE TABLE heap (a int, b int);
