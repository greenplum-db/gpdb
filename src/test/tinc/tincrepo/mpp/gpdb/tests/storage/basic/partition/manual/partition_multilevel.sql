set enable_partition_fast_insert = on;
set enable_partition_rules = off;
set gp_enable_hash_partitioned_tables = true;

--------------------
-- MULTIPARTITION
CREATE TABLE MULTI_PART (a int, b date, c char, d char(4), e varchar(20), f timestamp)
partition by range (b)
subpartition by list (a) subpartition template ( subpartition l1 values (1,2,3,4,5), subpartition l2 values (6,7,8,9,10) ),
subpartition by list (e) subpartition template ( subpartition ll1 values ('Engineering'), subpartition ll2 values ('QA') ),
subpartition by hash (c) subpartitions 2
(
  start (date '2007-01-01')
  end (date '2010-01-01') every (interval '1 year')
);
alter table multi_part add default partition default_part;
-- FAILED, MPP-3250
insert into multi_part values (1,'2007-01-01','a','aaaa','aaaaaa','2007-01-01 00:00:00');

CREATE TABLE MULTI_PART2(a int, b int, c int, d int, e int, f int, g int, h int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (10) every (1) ),
subpartition by range (c) subpartition template ( start (1) end (10) every (1), default partition cc ),
subpartition by range (d) subpartition template ( start (1) end (10) every (1), default partition dd ),
subpartition by range (e) subpartition template ( start (1) end (10) every (1), default partition ee ),
subpartition by range (f) subpartition template ( start (1) end (10) every (1), default partition ff ),
subpartition by range (g) subpartition template ( start (1) end (10) every (1), default partition gg ),
subpartition by range (h) subpartition template ( start (1) end (10) every (1), default partition hh ),
subpartition by range (i) subpartition template ( start (1) end (10) every (1), default partition ii ),
subpartition by range (j) subpartition template ( start (1) end (10) every (1), default partition jj ),
subpartition by range (k) subpartition template ( start (1) end (10) every (1), default partition kk ),
subpartition by range (l) subpartition template ( start (1) end (10) every (1), default partition ll ),
subpartition by range (m) subpartition template ( start (1) end (10) every (1), default partition mm ),
subpartition by range (n) subpartition template ( start (1) end (10) every (1), default partition nn ),
subpartition by range (o) subpartition template ( start (1) end (10) every (1), default partition oo ),
subpartition by range (p) subpartition template ( start (1) end (10) every (1), default partition pp ),
subpartition by range (q) subpartition template ( start (1) end (10) every (1), default partition qq ),
subpartition by range (r) subpartition template ( start (1) end (10) every (1), default partition rr ),
subpartition by range (s) subpartition template ( start (1) end (10) every (1), default partition ss ),
subpartition by range (t) subpartition template ( start (1) end (10) every (1), default partition tt ),
subpartition by range (u) subpartition template ( start (1) end (10) every (1), default partition uu ),
subpartition by range (v) subpartition template ( start (1) end (10) every (1), default partition vv ),
subpartition by range (w) subpartition template ( start (1) end (10) every (1), default partition ww ),
subpartition by range (x) subpartition template ( start (1) end (10) every (1), default partition xx ),
subpartition by range (y) subpartition template ( start (1) end (10) every (1), default partition yy ),
subpartition by range (z) subpartition template ( start (1) end (10) every (1), default partition zz ),
( start (1) end (10) every (1) );

-- Out of memory
CREATE TABLE MULTI_PART2(a int, b int, c int, d int, e int, f int, g int, h int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
partition by range (a)
subpartition by range (b) subpartition ( start (1) end (10) every (1) ),
subpartition by range (c) subpartition ( start (1) end (10) every (1) ),
subpartition by range (d) subpartition ( start (1) end (10) every (1) ),
subpartition by range (e) subpartition ( start (1) end (10) every (1) ),
subpartition by range (f) subpartition ( start (1) end (10) every (1) ),
subpartition by range (g) subpartition ( start (1) end (10) every (1) ),
subpartition by range (h) subpartition ( start (1) end (10) every (1) ),
subpartition by range (i) subpartition ( start (1) end (10) every (1) ),
subpartition by range (j) subpartition ( start (1) end (10) every (1) ),
subpartition by range (k) subpartition ( start (1) end (10) every (1) ),
subpartition by range (l) subpartition ( start (1) end (10) every (1) ),
subpartition by range (m) subpartition ( start (1) end (10) every (1) ),
subpartition by range (n) subpartition ( start (1) end (10) every (1) ),
subpartition by range (o) subpartition ( start (1) end (10) every (1) ),
subpartition by range (p) subpartition ( start (1) end (10) every (1) ),
subpartition by range (q) subpartition ( start (1) end (10) every (1) ),
subpartition by range (r) subpartition ( start (1) end (10) every (1) ),
subpartition by range (s) subpartition ( start (1) end (10) every (1) ),
subpartition by range (t) subpartition ( start (1) end (10) every (1) ),
subpartition by range (u) subpartition ( start (1) end (10) every (1) ),
subpartition by range (v) subpartition ( start (1) end (10) every (1) ),
subpartition by range (w) subpartition ( start (1) end (10) every (1) ),
subpartition by range (x) subpartition ( start (1) end (10) every (1) ),
subpartition by range (y) subpartition ( start (1) end (10) every (1) ),
subpartition by range (z) subpartition ( start (1) end (10) every (1) ),
( start (1) end (10) every (1) );


-- FAILED at level 10 (j)
CREATE TABLE MULTI_PART2(a int, b int, c int, d int, e int, f int, g int, h int, i int, j int, k int, l int, m int, n int, o int, p int, q int, r int, s int, t int, u int, v int, w int, x int, y int, z int)
distributed by (a)
partition by range (a)
subpartition by range (b) subpartition template ( start (1) end (2) every (1)),
subpartition by range (c) subpartition template ( start (1) end (2) every (1)),
subpartition by range (d) subpartition template ( start (1) end (2) every (1)),
subpartition by range (e) subpartition template ( start (1) end (2) every (1)),
subpartition by range (f) subpartition template ( start (1) end (2) every (1)),
subpartition by range (g) subpartition template ( start (1) end (2) every (1)),
subpartition by range (h) subpartition template ( start (1) end (2) every (1)),
subpartition by range (i) subpartition template ( start (1) end (2) every (1)),
subpartition by range (j) subpartition template ( start (1) end (2) every (1)),
subpartition by range (k) subpartition template ( start (1) end (2) every (1)),
subpartition by range (l) subpartition template ( start (1) end (2) every (1)),
subpartition by range (m) subpartition template ( start (1) end (2) every (1)),
subpartition by range (n) subpartition template ( start (1) end (2) every (1)),
subpartition by range (o) subpartition template ( start (1) end (2) every (1)),
subpartition by range (p) subpartition template ( start (1) end (2) every (1)),
subpartition by range (q) subpartition template ( start (1) end (2) every (1)),
subpartition by range (r) subpartition template ( start (1) end (2) every (1)),
subpartition by range (s) subpartition template ( start (1) end (2) every (1)),
subpartition by range (t) subpartition template ( start (1) end (2) every (1)),
subpartition by range (u) subpartition template ( start (1) end (2) every (1)),
subpartition by range (v) subpartition template ( start (1) end (2) every (1)),
subpartition by range (w) subpartition template ( start (1) end (2) every (1)),
subpartition by range (x) subpartition template ( start (1) end (2) every (1)),
subpartition by range (y) subpartition template ( start (1) end (2) every (1)),
subpartition by range (z) subpartition template ( start (1) end (2) every (1))
( start (1) end (2) every (1));

