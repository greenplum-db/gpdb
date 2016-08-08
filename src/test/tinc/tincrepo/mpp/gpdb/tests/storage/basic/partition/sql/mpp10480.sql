--start_ignore
drop table mpp10480_partsupp_partition;
--end_ignore
CREATE TABLE mpp10480_partsupp_partition ( ps_partkey int,
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

\!pg_dump --no-owner -t mpp10480_partsupp_partition -s -O -d @DBNAME@ > @out_dir@/mpp10480-pg_dump.out
