-- test for Github Issue 15278
-- QD should reset InterruptHoldoffCount
create table t_15278(a int, b int);
insert into t_15278 values (-1,1);

begin;
declare c1 cursor for select count(*) from t_15278 group by sqrt(a);
abort;

-- Without fix, the above transaction will lead
-- QD's global var InterruptHoldoffCount not reset to 0
-- thus the below SQL will return t. After fixing, now
-- the below SQL will print an error message, this is
-- the correct behavior.
select pg_cancel_backend(pg_backend_pid());

drop table t_15278;

