-- start-ignore

drop schema if exists bm_ao cascade;
create schema bm_ao;
set search_path to bm_ao;

-- end_ignore

CREATE TABLE outer_table (
    flex_value_id numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    language character varying(4) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    last_update_date date ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    last_updated_by numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    creation_date date ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    created_by numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    last_update_login numeric(10,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    description character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    source_lang character varying(4) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    flex_value_meaning character varying(150) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    legalhold character varying(5) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    legaldescription character varying(255) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    dataowner character varying(255) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    purgedate date ENCODING (compresstype=None,compresslevel=1,blocksize=32768)
)
WITH (appendonly=true, orientation=column, compresstype=None) DISTRIBUTED BY (flex_value_id);


set allow_system_table_mods="DML";
UPDATE pg_class
SET
	relpages = 1::int,
	reltuples = 0.0::real
WHERE relname = 'outer_table' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'ofa_applsys');
UPDATE pg_class
SET
	relpages = 64::int,
	reltuples = 61988.0::real
WHERE relname = 'outer_table' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'applsys');

CREATE TABLE inner_table (
    flex_value_set_id numeric(10,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    flex_value_id numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    flex_value character varying(150) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    last_update_date date ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    last_updated_by numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    creation_date date ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    created_by numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    last_update_login numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    enabled_flag character varying(1) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    summary_flag character varying(1) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    start_date_active date ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    end_date_active date ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    parent_flex_value_low character varying(60) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    parent_flex_value_high character varying(60) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    structured_hierarchy_level numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    hierarchy_level character varying(30) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    compiled_value_attributes character varying(2000) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    value_category character varying(30) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute1 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute2 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute3 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute4 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute5 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute6 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute7 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute8 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute9 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute10 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute11 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute12 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute13 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute14 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute15 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute16 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute17 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute18 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute19 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute20 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute21 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute22 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute23 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute24 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute25 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute26 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute27 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute28 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute29 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute30 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute31 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute32 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute33 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute34 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute35 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute36 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute37 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute38 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute39 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute40 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute41 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute42 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute43 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute44 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute45 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute46 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute47 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute48 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute49 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute50 character varying(240) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    attribute_sort_order numeric(15,0) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    legalhold character varying(5) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    legaldescription character varying(255) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    dataowner character varying(255) ENCODING (compresstype=None,compresslevel=1,blocksize=32768),
    purgedate date ENCODING (compresstype=None,compresslevel=1,blocksize=32768)
)
WITH (appendonly=true, orientation=column, compresstype=None) DISTRIBUTED BY (flex_value_set_id ,flex_value_id);

CREATE INDEX inner_table_idx ON inner_table USING btree (flex_value_id);
UPDATE pg_class
SET
	relpages = 64::int,
	reltuples = 30994.0::real
WHERE relname = 'inner_table' AND relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = 'applsys');


insert into outer_table values(12345,'ZHS');
insert into outer_table values(012345,'ZHS');
insert into inner_table values(54321,12345);
insert into inner_table values(654321,123456);

CREATE TABLE foo(fid numeric(10,0));
insert into foo VALUES(54321);

-- start_ignore

set optimizer_segments=64;
set optimizer_enable_bitmapscan=on; 
set optimizer_enable_hashjoin=off;
select disable_xform('CXformInnerJoin2IndexGetApply');

-- end_ignore


SELECT fid FROM
(SELECT b1.flex_value_set_id, b1.flex_value_id
FROM
inner_table b1,
outer_table c1
WHERE
b1.flex_value_id = c1.flex_value_id 
and c1.language = 'ZHS') bar,
foo
WHERE foo.fid = bar.flex_value_set_id;

-- double free mixed bitmap indexes (StreamBitmap with TIDBitmap)
CREATE TABLE bmcrash (
    btree_col2 date DEFAULT now(),
    bitmap_col text NOT NULL,
    btree_col1 character varying(50) NOT NULL,
    dist_col serial
)
WITH (appendonly=true, compresslevel=5, compresstype=zlib) DISTRIBUTED BY (dist_col);

CREATE INDEX bm_idx ON bmcrash USING bitmap (bitmap_col);
CREATE INDEX bt_idx ON bmcrash USING btree (btree_col1);
CREATE INDEX bt_idx_2 ON bmcrash USING btree (btree_col2);

INSERT INTO bmcrash (btree_col2, bitmap_col, btree_col1)
SELECT date '2015-01-01' + (i % (365 * 2)), i % 1000, 'abcdefg' || (i% 1000)
from generate_series(1,10000) as i;

select count(1) from bmcrash where bitmap_col = '999' AND btree_col1 = 'abcdefg999';
select count(1) from bmcrash where bitmap_col = '999' OR btree_col1 = 'abcdefg999';
select count(1) from bmcrash where bitmap_col = '999' OR btree_col1 = 'abcdefg999' AND btree_col2 = '2015-01-01';
select count(1) from bmcrash where bitmap_col = '999' AND btree_col1 = 'abcdefg999' OR btree_col2 = '2015-01-01';

select count(1) from bmcrash where btree_col1 = 'abcdefg999' AND bitmap_col = '999';
select count(1) from bmcrash where btree_col1 = 'abcdefg999' OR bitmap_col = '999' AND btree_col2 = '2015-01-01';
select count(1) from bmcrash where btree_col1 = 'abcdefg999' AND bitmap_col = '999' OR btree_col2 = '2015-01-01';
select count(1) from bmcrash where btree_col1 = 'abcdefg999' AND btree_col2 = '2015-01-01' OR bitmap_col = '999';
select count(1) from bmcrash where btree_col1 = 'abcdefg999' AND btree_col2 = '2015-01-01' AND bitmap_col = '999';
select count(1) from bmcrash where btree_col1 = 'abcdefg999' OR btree_col2 = '2015-01-01' AND bitmap_col = '999';
select count(1) from bmcrash where btree_col1 = 'abcdefg999' OR btree_col2 = '2015-01-01' OR bitmap_col = '999';

set enable_bitmapscan=on; 
set enable_hashjoin=off; 
set enable_indexscan=on; 
set enable_nestloop=on; 
set enable_seqscan=off;

select count(1) from bmcrash where btree_col1 = 'abcdefg999' AND bitmap_col = '999' OR bitmap_col = '888' OR btree_col2 = '2015-01-01';
select count(1) from bmcrash b1, bmcrash b2 where b1.bitmap_col = b2.bitmap_col or b1.bitmap_col = '999' and b1.btree_col1 = 'abcdefg999';

set optimizer_enable_hashjoin=off;
with bm as (select * from bmcrash where btree_col1 = 'abcdefg999' AND bitmap_col = '999' OR bitmap_col = '888' OR btree_col2 = '2015-01-01')
select count(1) from bm b1, bm b2 where b1.dist_col = b2.dist_col;

-- qual with like, any.
with bm as (select * from bmcrash where (btree_col1 like 'abcde%') AND bitmap_col in ('999', '888'))
select count(1) from bm b1, bm b2 where b1.dist_col = b2.dist_col;

-- start_ignore
drop schema bm_ao cascade;
-- end_ignore
