create table t_concurrent_splitupdate_with_norm_upddel (a int, b int) distributed by (a);

insert into t_concurrent_splitupdate_with_norm_upddel values (1, 1);

1: begin;
1: update t_concurrent_splitupdate_with_norm_upddel set a = 2 where b = 1;

2: begin;
2&: delete from t_concurrent_splitupdate_with_norm_upddel;

1: end;
2<:

1q:
2:abort;
2q:


