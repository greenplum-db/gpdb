--start_ignore
drop table if exists pt_tab_encode cascade;
--end_ignore

CREATE TABLE pt_tab_encode (a int, b text)
with (appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1)
distributed by (a)
partition by list(b)
(
    partition s_abc values ('abc') with (appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1)
);

alter table pt_tab_encode add partition "s_xyz" values ('xyz') WITH (appendonly=true, orientation=column, compresstype=quicklz, compresslevel=1);

select tablename, partitiontablename from pg_partitions where tablename = 'pt_tab_encode';

select gp_segment_id, attrelid::regclass, attnum, attoptions from pg_attribute_encoding where attrelid = 'pt_tab_encode_1_prt_s_abc'::regclass;
select gp_segment_id, attrelid::regclass, attnum, attoptions from gp_dist_random('pg_attribute_encoding') where attrelid = 'pt_tab_encode_1_prt_s_abc'::regclass order by 1,3 limit 5;

select gp_segment_id, attrelid::regclass, attnum, attoptions from pg_attribute_encoding where attrelid = 'pt_tab_encode_1_prt_s_xyz'::regclass;
select gp_segment_id, attrelid::regclass, attnum, attoptions from gp_dist_random('pg_attribute_encoding') where attrelid = 'pt_tab_encode_1_prt_s_xyz'::regclass order by 1,3 limit 5;

select oid::regclass, relkind, relstorage, reloptions from pg_class where oid = 'pt_tab_encode_1_prt_s_abc'::regclass;
select oid::regclass, relkind, relstorage, reloptions from pg_class where oid = 'pt_tab_encode_1_prt_s_xyz'::regclass;

insert into pt_tab_encode select 1, 'xyz' from generate_series(1, 100000);
insert into pt_tab_encode select 1, 'abc' from generate_series(1, 100000);

select pg_size_pretty(pg_relation_size('"pt_tab_encode_1_prt_s_abc"')), get_ao_compression_ratio('"pt_tab_encode_1_prt_s_abc"');
select pg_size_pretty(pg_relation_size('"pt_tab_encode_1_prt_s_xyz"')), get_ao_compression_ratio('"pt_tab_encode_1_prt_s_xyz"');

