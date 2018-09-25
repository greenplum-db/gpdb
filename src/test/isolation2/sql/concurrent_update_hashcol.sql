-- test for heap table
create table tab_update_hashcol (c1 int, c2 int) distributed by(c1);
insert into tab_update_hashcol select i, i from generate_series(1, 10)i;

select * from tab_update_hashcol order by 1;

1: begin;
2: begin;

1: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;

2&: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;

1: end;
2<:
2: end;

select * from tab_update_hashcol order by 1;

drop table tab_update_hashcol;

-- test for ao table
create table tab_update_hashcol (c1 int, c2 int) with(appendonly=true) distributed by(c1);
insert into tab_update_hashcol select i, i from generate_series(1, 10)i;

select * from tab_update_hashcol order by 1;

1: begin;
2: begin;

1: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;

2&: update tab_update_hashcol set c1 = c1 + 1 where c1 = 1;

1: end;
2<:
2: end;

select * from tab_update_hashcol order by 1;

drop table tab_update_hashcol;

1q:
2q:
