
CREATE TABLE heap_hybrid_part (
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
 subpartition sp1 start('1') end('3030')  ,
 subpartition sp2 end('6096') inclusive ,
 subpartition sp3   start('6096') exclusive end('7201') ,
 subpartition sp4 end('10001')  
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') ,        
subpartition sp2 start('4139')  ,
subpartition sp3 start('4685')   ,
subpartition sp4 start('5675') )
);

create index heap_hybrid_part_btree_index on heap_hybrid_part (PS_PARTKEY);

INSERT INTO heap_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));



CREATE TABLE ao_hybrid_part (
                PS_PARTKEY INTEGER,
                PS_SUPPKEY INTEGER,
                PS_AVAILQTY integer,
                PS_SUPPLYCOST decimal,
                PS_COMMENT VARCHAR(199)
                ) WITH (checksum=false, appendonly=true,blocksize=1171456,compresslevel=3)
 partition by range (ps_supplycost) 
 subpartition by range (ps_suppkey)
,subpartition by range (ps_partkey) subpartition template (start('1') end('200001') every(66666)

)
(
partition p1 start('1') end('20') inclusive
(
 subpartition sp1 start('1') end('3030')  WITH (checksum=false, appendonly=true,blocksize=1171456, compresstype = zlib, compresslevel=6, fillfactor=60),
 subpartition sp2 end('6096') inclusive WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3),
 subpartition sp3   start('6096') exclusive end('7201') with (orientation='column',appendonly=true, compresstype = zlib, compresslevel=5, fillfactor=60),
 subpartition sp4 end('10001')  WITH (checksum=false,orientation='column',appendonly=true, blocksize=1171456,compresslevel=7,fillfactor=40)
), 
partition p2 end('1001') inclusive
(
subpartition sp1 start('1') WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=8) ,        
subpartition sp2 start('4139') with (orientation='column',appendonly=true, compresstype = zlib, compresslevel=5, fillfactor=60) ,
subpartition sp3 start('4685')  WITH (checksum=false,appendonly=true, blocksize=1171456,compresslevel=3) ,
subpartition sp4 start('5675') WITH (checksum=false,orientation='column',appendonly=true, blocksize=1171456,compresslevel=9))
);

create index ao_hybrid_part_btree_index on ao_hybrid_part (PS_PARTKEY);

INSERT INTO ao_hybrid_part  VALUES( generate_series(1,500), generate_series(500,1000),generate_series(600,1100),591.18,substring( 'final accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously afterfinal accounts around the blithely special asymptotes wake carefully beyond the bold dugouts. regular ideas haggle furiously after' for (random()*150)));


