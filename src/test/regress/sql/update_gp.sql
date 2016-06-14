-- Test DELETE and UPDATE on an inherited table.
-- The special aspect of this table is that the inherited table has
-- a different distribution key. 'p' table's distribution key matches
-- that of 'r', but 'p2's doesn't. Test that the planner adds a Motion
-- node correctly for p2.
create table todelete (a int) distributed by (a);
create table parent (a int, b int, c int) distributed by (a);
create table child (a int, b int, c int) inherits (parent) distributed by (b);

insert into parent select g, g, g from generate_series(1,5) g;
insert into child select g, g, g from generate_series(6,10) g;

insert into todelete select generate_series(3,4);

delete from parent using todelete where parent.a = todelete.a;

insert into todelete select generate_series(5,7);

update parent set c=c+100 from todelete where parent.a = todelete.a;

select * from parent;

drop table todelete;
drop table child;
drop table parent;

-- This is similar to the above, but with a partitioned table (which is
-- implemented by inheritance) rather than an explicitly inherited table.
-- The scans on some of the partitions degenerate into Result nodes with
-- False one-time filter, which don't need a Motion node.
create table todelete (a int, b int) distributed by (a);
create table target (a int, b int, c int)
        distributed by (a)
        partition by range (c) (start(1) end(5) every(1), default partition extra);

insert into todelete select g, g % 4 from generate_series(1, 10) g;
insert into target select g, 0, 3 from generate_series(1, 5) g;
insert into target select g, 0, 1 from generate_series(1, 5) g;

delete from target where c = 3 and a in (select b from todelete);

insert into todelete values (1, 5);

update target set b=target.b+100 where c = 3 and a in (select b from todelete);

select * from target;

drop table todelete;
drop table target;

