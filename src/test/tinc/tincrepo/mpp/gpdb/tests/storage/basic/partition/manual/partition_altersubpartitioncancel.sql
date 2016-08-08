-- MPP-6098
CREATE TABLE partsupp ( ps_partkey %data%,
ps_suppkey integer, ps_availqty integer,
ps_supplycost numeric, ps_comment character varying(199) )
PARTITION BY RANGE(ps_partkey)
subpartition by range (ps_partkey)
subpartition template
( subpartition sp1 start(0) end (100) every(10),
  subpartition sp2 start(100) end (200) every(10),
  subpartition sp3 start(200) end (300) every(10),
  subpartition sp4 start(300) end (400) every(10)
)
( partition aa start (0) end (400) every(100)
);

alter table partsupp set subpartition template ();
alter table partsupp set subpartition template ( subpartition sp5 start(0) end (10000) every(100) ); -- Cancel after 1-2 seconds
alter table partsupp set subpartition template ( subpartition sp5 start(0) end (10000) every(100) ); -- Cancel after 1-2 seconds
alter table partsupp set subpartition template ( subpartition sp5 start(0) end (10000) every(100) ); -- Cancel after 1-2 seconds

alter table partsupp set subpartition template ( subpartition sp5 start(0) end (10000000) every(1) ); -- Lots of subpartition template and cancel
