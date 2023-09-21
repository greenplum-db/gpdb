--
-- Test Cases to check Index scan, Index Only scan,
-- Dynamic Index scan & Dynamic Index only scan costing
--


---------------------------------------------------------------------
-- Scenario1: Verify impact of unused index column in predicate,
-- which helps in selecting best index in a multiple index scenario.
---------------------------------------------------------------------

    -- Case A : Index - idx_ab, Columns used in predicate : a
    -- Case B : Index - idx_ab, Columns used in predicate : b
    -- In the above two cases, the index scan using idx_ab should be higher
    -- in CASE B, compared for CASE A, as the in Case-B, index column
    -- of higher significance is unused  i.e. col 'a'

drop table if exists foo;
create table foo (a int , b int, c int) distributed by (a);
insert into foo select i,i,i from generate_series(1,5)i;
-- Adding 1,1,1 in the table so that we have different values for NDV and table rows.
insert into foo select 1,1,1*i/i from generate_series(1,5)i;

-- 1.1 Test case for index scans
CREATE INDEX idx_foo_ab ON foo USING btree(a,b);
CREATE INDEX idx_foo_ba ON foo USING btree(b,a);
analyze foo;
    -- index idx_foo_ab should be selected
explain select * from foo where a=1;
    -- Index idx_foo_ba should be selected
explain select * from foo where b=1;
drop index idx_foo_ab;
drop index idx_foo_ba;

-- 1.2 Test case for index only scans
CREATE INDEX idx_foo_abc ON foo USING btree(a,b,c);
CREATE INDEX idx_foo_cba ON foo USING btree(c,b,a);
vacuum analyze foo;
    --  Index idx_foo_abc should be selected
explain select * from foo where a=1;
    --  Index idx_foo_cba should be selected
explain select * from foo where c=1;
drop index idx_foo_abc;
drop index idx_foo_cba;


-- 1.3 Test case for dynamic index scans
drop table if exists foo;
drop table if exists bar_PT;
create table foo (a int , b int, c int) distributed by (a);
insert into foo select i,i,i from generate_series(1,5)i;
analyze foo;
    -- Partitioned Table
create table bar_PT (a int, b int, c int) partition by range(a) (start (1) inclusive end (12) every (2)) distributed by (a);
insert into bar_PT select i,i,i from generate_series(1,11)i;
CREATE INDEX idx_bar_PT_ab ON bar_PT USING btree(a,b);
CREATE INDEX idx_bar_PT_ba ON bar_PT USING btree(b,a);
analyze bar_PT;
    --  Index idx_bar_PT_ab should be selected
explain select * from bar_PT join foo on bar_PT.a =foo.a;
    --  Index idx_bar_PT_ba should be selected
explain select * from bar_PT join foo on bar_PT.b =foo.b;
drop index idx_bar_PT_ab;
drop index idx_bar_PT_ba;

-- 1.4 Test case for dynamic index only scans
CREATE INDEX idx_bar_PT_abc ON bar_PT USING btree(a,b,c);
CREATE INDEX idx_bar_PT_cba ON bar_PT USING btree(c,b,a);
vacuum analyze bar_PT;
    --  Index idx_bar_PT_abc should be selected
explain select * from bar_PT join foo on bar_PT.a =foo.a;
    --  Index idx_bar_PT_cba should be selected
explain select * from bar_PT join foo on bar_PT.c =foo.c;
drop table if exists foo;
drop table if exists bar_PT;

------------------------------------------------
-- Scenario2: Unindexed predicate column in index.
------------------------------------------------
-- If the index does not cover all the columns of the predicate, than a scan cost
-- using it should be higher compared to an index which covers all the predicate columns.
drop table if exists foo;
create table foo (a int , b int, c int) distributed by (a);
insert into foo select i,i,i from generate_series(1,5)i;
-- Adding 1,1,1 in the table so that we have different values for NDV and table rows.
insert into foo select 1,1,1*i/i from generate_series(1,5)i;

-- 2.1 Test case for index scans
CREATE INDEX idx_foo_a ON foo USING btree(a);
CREATE INDEX idx_foo_abc ON foo USING btree(a,b,c);
vacuum analyze foo;

-- Query1 - Index idx_foo_a should be selected.
explain select * from foo where a=1;
-- Query2 - Index idx_foo_abc should be selected.
explain select * from foo where a=1 and b=1 and c=1;
drop index idx_foo_a;
drop index idx_foo_abc;

-- 2.2 Test case for dynamic index scans
drop table if exists foo;
drop table if exists bar_PT;
create table foo (a int , b int, c int) distributed by (a);
insert into foo select i,i,i from generate_series(1,5)i;
analyze foo;
    -- Partitioned Table
create table bar_PT (a int, b int, c int) partition by range(a) (start (1) inclusive end (12) every (2)) distributed by (a);
insert into bar_PT select i,i,i from generate_series(1,11)i;
CREATE INDEX idx_bar_PT_a ON bar_PT USING btree(a);
CREATE INDEX idx_bar_PT_abc ON bar_PT USING btree(a,b,c);
vacuum analyze bar_PT;

-- Query1: Index idx_bar_PT_a should be selected.
explain select * from bar_PT join foo on bar_PT.a =foo.a ;
-- Query2 : Index idx_bar_PT_abc should be selected.
explain select * from bar_PT join foo on bar_PT.a =foo.a and bar_PT.b =foo.b and bar_PT.c =foo.c;
drop table if exists foo;
drop table if exists bar_PT;
