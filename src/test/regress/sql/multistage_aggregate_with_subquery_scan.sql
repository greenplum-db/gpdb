-- Given a table with a few columns
create table a (i int, j int, k int) distributed by (i);
-- When I try to query the table with multiple stage aggregate and a subquery
-- Then it should succeed instead of crashing
select count(distinct j), count(distinct k)
from (
     select j, k
     from a
     group by j,k
) some_subquery
group by j;
