--
-- BOOLEAN
--

create table gpd1 (c1 char(1), c2 numeric, c3 numeric) distributed by (c1);
insert into gpd1 values ('a', 1, 1);
analyze gpd1;
insert into gpd1 values ('a', 1, 2);
insert into gpd1 values ('b', 2, 1);
insert into gpd1 values ('b', 1, 2);
insert into gpd1 values ('c', 3, 2);
insert into gpd1 values ('c', 2, 3);
insert into gpd1 values ('d', 4, 4);
insert into gpd1 values ('d', 4, 3);
--
-- ignore
--
select c1 from gpd1;

--
-- order 1, 2
--
select c1, c1, c2, c3 from gpd1 order by 1,2;
--
--
select c1, c1, c2, c3 from gpd1 order by 1,2,3,4; -- order 1, 2     , 3    , 4
--
-- ignore
-- order 1, 2
--
select c1, c1, c2, c3 from gpd1 order by 1,2;

--
--  mvd 2,3->1 ; 2,3->4,5
-- order 4
--
select c1, c2, c3, c1, c1, c2 from gpd1 order by 4;

--  Brian: the contents of column 1 are not determined by any other 
--  column -- the column "specifies itself"
--
--  mvd 1->1
--
select row_number() over (), c1, c2, c3 from gpd1;

-- Brian: 1 specifies 2
--  
--
    select -- mvd 1 -> 2
        x,
        row_number() over (partition by x) as y,
        z 
    from (values (1,'A'),(1,'B'),(2,'C'),(2,'D')) r(x,z);

-- start_ignore
--
-- whatever is here is ignored until we reach end_ignore
--
-- end_ignore
--

-- explain testing
--
set gp_segments_for_planner=4;
set optimizer_segments=4;
set gp_cost_hashjoin_chainwalk=on;
set optimizer_nestloop_factor = 1.0;
explain analyze select a.* from gpd1 as a, gpd1 as b where b.c1 in (select max(c1) from gpd1);
explain select a.* from gpd1 as a, gpd1 as b where b.c1 in (select max(c1) from gpd1);
select a.* from gpd1 as a, gpd1 as b where b.c1 in (select max(c1) from gpd1);
set gp_segments_for_planner=40;
set optimizer_segments=40;
set optimizer_nestloop_factor = 1.0;
explain select a.* from gpd1 as a, gpd1 as b where b.c1 in (select max(c1) from gpd1);
select a.* from gpd1 as a, gpd1 as b where b.c1 in (select max(c1) from gpd1);
explain analyze select a.* from gpd1 as a, gpd1 as b where b.c1 in (select max(c1) from gpd1);

--
-- Clean up
--

DROP TABLE  gpd1;

-- start_matchsubs
--
-- # create a match/subs expression to handle a value which always changes
--
-- # use zero-width negative look-behind assertion to match "gpmatchsubs1"
-- # that does not follow substring
--
-- m/(?<!substring..)gpmatchsubs1/
-- s/gpmatchsubs1.*/happy sub1/
--
-- m/(?<!substring..)gpmatchsubs2/
-- s/gpmatchsubs2.*/happy sub2/
--
-- end_matchsubs

-- substitute constant values for results
-- use substring because length of time string varies which changes output
select substring('gpmatchsubs1' || now(), 1,  25);
select substring('gpmatchsubs2' || now(), 1,  25);

-- start_matchignore
--
-- # create a match expression to handle a value which always changes
--
-- m/(?<!substring..)gpmatchignore1/
-- m/(?<!substring..)gpmatchignore2/
--
-- end_matchignore

-- just ignore the results

select substring('gpmatchignore1' || now(), 1,  25);
select substring('gpmatchignore2' || now(), 1,  25);
reset optimizer_nestloop_factor;
-- 

