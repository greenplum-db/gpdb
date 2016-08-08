-- 
-- @created 2013-03-15 12:00:00
-- @modified 2012-03-15 12:00:00
-- @tags storage
-- @description AO concurrent create/drop sql
-- @concurrency 10
-- @iterations 20000

create table mmm( a int, b int ) with (appendonly=true) distributed by (a);
create table mmm1( a int, b int ) with (appendonly=true) distributed by (a);
create table mmm2( a int, b int ) distributed by (a);
drop table mmm,mmm1,mmm2;
create table mmm( a int, b int ) with (appendonly=true) distributed by (a);
create table mmm1( a int, b int ) with (appendonly=true) distributed by (a);
create table mmm2( a int, b int ) distributed by (a);
drop table mmm,mmm1,mmm2;
