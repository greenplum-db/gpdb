-- Test using an external table in a subquery.
--
-- We used to have a bug where the scan on the external table was not
-- broadcast to all nodes, so each segment scanned only its own portion
-- of the external table, when the scan was in a subquery. In that case,
-- the count(*) calculated for each value below was 1, but it should be
-- equal to the number of segments, because this external table produces
-- the same rows on every segment.

CREATE EXTERNAL WEB TABLE echotable (c1 int, c2 int, c3 int) EXECUTE
'echo "1,2,3"; echo "4,5,6";' FORMAT 'TEXT' (DELIMITER ',');

create table test_ext_foo (c1 int, c2 int4);
insert into test_ext_foo select g, g from generate_series(1, 20) g;

-- This should return 2 and 5, as the two rows are duplicated in
-- every segment (assuming you have at least two segments in your
-- cluster).
select c2 from echotable group by c2 having count(*) >= 2;

select * from test_ext_foo as o
where (select count(*) from echotable as i where i.c2 = o.c2) >= 2;

-- Planner test to make sure the initplan is not removed for function scan
explain select * from generate_series(1,2) s1 join pg_class on true where s1=(select pg_backend_pid());

-- Planner test: constant folding in subplan testexpr  produces no error
create table subselect_t1 (a int, b int, c int) distributed by (a);
create table subselect_t2 (a int, b int, c int) distributed by (a);

insert into subselect_t1 values (1,1,1);
insert into subselect_t2 values (1,1,1);

select * from subselect_t1 where NULL in (select c from subselect_t2);
select * from subselect_t1 where NULL in (select c from subselect_t2) and exists (select generate_series(1,2));

-- Planner test to make sure initplan is removed when no param is used
select * from subselect_t2 where false and exists (select generate_series(1,2));

