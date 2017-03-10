-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE TABLE ck_sync1_hybrid_part1 (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                ) 
 partition by range (ps_supplycost)
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  WITH (checksum=false,   appendonly=true,blocksize=1171456,compresslevel=3),
 subpartition sp2 end('6096') inclusive,
 subpartition sp3   start('6096') exclusive end('7201') with (orientation='column',appendonly=true),
 subpartition sp4 end('10001')  WITH (checksum=false,appendonly=true,     blocksize=1171456,compresslevel=3)
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,        
subpartition sp2 start('4139') with (orientation='column',appendonly=true) ,
subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3) ,
subpartition sp4 start('5675') )
);


COPY ck_sync1_hybrid_part1 FROM '/Users/aagrawal/workspace/5.0/gpdb/src/test/tinc/tincrepo/mpp/gpdb/tests/storage/filerep_end_to_end/hybrid_part.data' delimiter as '|';



CREATE TABLE ck_sync1_hybrid_part2 (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                ) 
 partition by range (ps_supplycost)
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  WITH (checksum=false,   appendonly=true,blocksize=1171456,compresslevel=3),
 subpartition sp2 end('6096') inclusive,
 subpartition sp3   start('6096') exclusive end('7201') with (orientation='column',appendonly=true),
 subpartition sp4 end('10001')  WITH (checksum=false,appendonly=true,     blocksize=1171456,compresslevel=3)
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,        
subpartition sp2 start('4139') with (orientation='column',appendonly=true) ,
subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3) ,
subpartition sp4 start('5675') )
);


COPY ck_sync1_hybrid_part2 FROM '/Users/aagrawal/workspace/5.0/gpdb/src/test/tinc/tincrepo/mpp/gpdb/tests/storage/filerep_end_to_end/hybrid_part.data' delimiter as '|';


CREATE TABLE ck_sync1_hybrid_part3 (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                ) 
 partition by range (ps_supplycost)
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  WITH (checksum=false,   appendonly=true,blocksize=1171456,compresslevel=3),
 subpartition sp2 end('6096') inclusive,
 subpartition sp3   start('6096') exclusive end('7201') with (orientation='column',appendonly=true),
 subpartition sp4 end('10001')  WITH (checksum=false,appendonly=true,     blocksize=1171456,compresslevel=3)
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,        
subpartition sp2 start('4139') with (orientation='column',appendonly=true) ,
subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3) ,
subpartition sp4 start('5675') )
);


COPY ck_sync1_hybrid_part3 FROM '/Users/aagrawal/workspace/5.0/gpdb/src/test/tinc/tincrepo/mpp/gpdb/tests/storage/filerep_end_to_end/hybrid_part.data' delimiter as '|';





CREATE TABLE ck_sync1_hybrid_part4 (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                ) 
 partition by range (ps_supplycost)
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  WITH (checksum=false,   appendonly=true,blocksize=1171456,compresslevel=3),
 subpartition sp2 end('6096') inclusive,
 subpartition sp3   start('6096') exclusive end('7201') with (orientation='column',appendonly=true),
 subpartition sp4 end('10001')  WITH (checksum=false,appendonly=true,     blocksize=1171456,compresslevel=3)
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,        
subpartition sp2 start('4139') with (orientation='column',appendonly=true) ,
subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3) ,
subpartition sp4 start('5675') )
);


COPY ck_sync1_hybrid_part4 FROM '/Users/aagrawal/workspace/5.0/gpdb/src/test/tinc/tincrepo/mpp/gpdb/tests/storage/filerep_end_to_end/hybrid_part.data' delimiter as '|';


CREATE TABLE ck_sync1_hybrid_part5 (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                ) 
 partition by range (ps_supplycost)
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  WITH (checksum=false,   appendonly=true,blocksize=1171456,compresslevel=3),
 subpartition sp2 end('6096') inclusive,
 subpartition sp3   start('6096') exclusive end('7201') with (orientation='column',appendonly=true),
 subpartition sp4 end('10001')  WITH (checksum=false,appendonly=true,     blocksize=1171456,compresslevel=3)
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,        
subpartition sp2 start('4139') with (orientation='column',appendonly=true) ,
subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3) ,
subpartition sp4 start('5675') )
);


COPY ck_sync1_hybrid_part5 FROM '/Users/aagrawal/workspace/5.0/gpdb/src/test/tinc/tincrepo/mpp/gpdb/tests/storage/filerep_end_to_end/hybrid_part.data' delimiter as '|';



CREATE TABLE ck_sync1_hybrid_part6 (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                ) 
 partition by range (ps_supplycost)
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  WITH (checksum=false,   appendonly=true,blocksize=1171456,compresslevel=3),
 subpartition sp2 end('6096') inclusive,
 subpartition sp3   start('6096') exclusive end('7201') with (orientation='column',appendonly=true),
 subpartition sp4 end('10001')  WITH (checksum=false,appendonly=true,     blocksize=1171456,compresslevel=3)
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,        
subpartition sp2 start('4139') with (orientation='column',appendonly=true) ,
subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3) ,
subpartition sp4 start('5675') )
);


COPY ck_sync1_hybrid_part6 FROM '/Users/aagrawal/workspace/5.0/gpdb/src/test/tinc/tincrepo/mpp/gpdb/tests/storage/filerep_end_to_end/hybrid_part.data' delimiter as '|';



CREATE TABLE ck_sync1_hybrid_part7 (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                ) 
 partition by range (ps_supplycost)
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  WITH (checksum=false,   appendonly=true,blocksize=1171456,compresslevel=3),
 subpartition sp2 end('6096') inclusive,
 subpartition sp3   start('6096') exclusive end('7201') with (orientation='column',appendonly=true),
 subpartition sp4 end('10001')  WITH (checksum=false,appendonly=true,     blocksize=1171456,compresslevel=3)
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,        
subpartition sp2 start('4139') with (orientation='column',appendonly=true) ,
subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3) ,
subpartition sp4 start('5675') )
);


COPY ck_sync1_hybrid_part7 FROM '/Users/aagrawal/workspace/5.0/gpdb/src/test/tinc/tincrepo/mpp/gpdb/tests/storage/filerep_end_to_end/hybrid_part.data' delimiter as '|';



DROP TABLE sync1_hybrid_part2;
DROP TABLE ck_sync1_hybrid_part1;

