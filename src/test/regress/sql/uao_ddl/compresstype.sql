-- Test compress types.  The objective is to verify basic
-- insert/update/delete works and NOT to test compression algorithm.
-- So don't bother inserting large data and checking compression
-- ratio.
BEGIN;
CREATE TABLE uao_tab_compress_none (
		  col_int int,
		  col_text text,
		  col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=NONE) distributed by (col_int);
insert into uao_tab_compress_none values
 (1, 'abc', 1), (2, 'pqr', 2), (3, 'lmn', 3);
delete from uao_tab_compress_none;
select count(*) = 0 as passed from uao_tab_compress_none;
SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is
not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM
pg_class WHERE relname='uao_tab_compress_none');

-- create uao table with compress=zlib COMPRESSLEVEL=1
CREATE TABLE uao_tab_compress_zlib1 (
		  col_int int,
		  col_text text,
		  col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=zlib, COMPRESSLEVEL=1)
 distributed by (col_int);

insert into uao_tab_compress_zlib1 select i , 'This is news of today:
Deadlock between Republicans and Democrats over how best to reduce the
U.S. deficit, and over what period, has blocked an agreement to allow
the raising of the $14.3 trillion debt ceiling '||i, (random() *
10000000)::numeric + 10000000 from GENERATE_SERIES(1000, 1999) AS i;
\d+ uao_tab_compress_zlib1
select count(*) from uao_tab_compress_zlib1;
update uao_tab_compress_zlib1 set col_text = 'New prefix ' || col_text;
delete from uao_tab_compress_zlib1 where col_int > 1500;
select count(*) from uao_tab_compress_zlib1;
-- Decompress tuples
select sum(length(col_text)) from uao_tab_compress_zlib1
 where col_int < 1100;
SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is
not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM
pg_class WHERE relname='uao_tab_compress_zlib1');

SELECT 1 AS compression_present from pg_appendonly WHERE
compresstype='zlib' AND compresslevel=1 AND relid=(SELECT oid FROM
pg_class WHERE relname='uao_tab_compress_zlib1');

-- create uao table with compress=zlib COMPRESSLEVEL=9
CREATE TABLE uao_tab_compress_zlib9 (
		  col_int int,
		  col_text text,
		  col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=zlib, COMPRESSLEVEL=9)
 distributed by (col_int);

insert into uao_tab_compress_zlib9 select i , 'This is news of today:
Deadlock between Republicans and Democrats over how best to reduce the
U.S. deficit, and over what period, has blocked an agreement to allow
the raising of the $14.3 trillion debt ceiling '||i, (random() *
10000000)::numeric + 10000000 from GENERATE_SERIES(1000, 1999) AS i;
\d+ uao_tab_compress_zlib9
select count(*) from uao_tab_compress_zlib9;
update uao_tab_compress_zlib9 set col_numeric = -col_int,
 col_text = 'New prefix ' || col_text;
delete from uao_tab_compress_zlib9 where col_int > 1500;
select count(*) from uao_tab_compress_zlib9;
-- Decompress tuples
select sum(length(col_text)) from uao_tab_compress_zlib9
 where col_int < 1100;
set gp_select_invisible = true;
select count(*) from uao_tab_compress_zlib9;
set gp_select_invisible = false;

SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is
not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM
pg_class WHERE relname='uao_tab_compress_zlib9');

SELECT 1 AS compression_present from pg_appendonly WHERE
compresstype='zlib' AND compresslevel=9 AND relid=(SELECT oid FROM
pg_class WHERE relname='uao_tab_compress_zlib9');
COMMIT;

-- Verify that RLE compression is not allowed for row oriented
-- append-optimized tables.
CREATE TABLE sto_ao_neg_rle (
		  col_int int,
		  col_text text,
		  col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=rle_type)
 distributed by (col_int);
