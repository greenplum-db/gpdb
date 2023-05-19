-- test that distributing or hash partitioning by an enum field or expression is blocked

CREATE DATABASE ban_enum;
\c ban_enum

-- create a test enum
create type colorEnum as enum ('r', 'g', 'b');

-- hash partition by enum column name
create table part (a int, b colorEnum) partition by hash(b);

-- hash partition by enum column expression
create table part (a int, b colorEnum) partition by hash((b));

-- distribute by enum column
create table distr (a colorEnum, b int);


-- clean up database
drop type colorEnum;
\c regression
DROP DATABASE ban_enum;
