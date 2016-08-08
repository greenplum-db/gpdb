-- Negative: using SCATTER BY in create table DML
-- The following should fail

    CREATE TABLE scatter_test1 (a int, b int)
    SCATTER BY a;

    CREATE TABLE scatter_test1 (a int, b int)
    SCATTER BY a,b;

    CREATE TABLE scatter_test1 (a int, b int)
    SCATTER BY (a);

    CREATE TABLE scatter_test1 (a int, b int)
    SCATTER BY (a,b);


