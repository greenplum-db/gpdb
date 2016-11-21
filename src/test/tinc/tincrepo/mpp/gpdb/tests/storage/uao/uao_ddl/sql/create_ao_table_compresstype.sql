-- @description : Create Updatable AO tables , with various compression type
-- 


-- create select uao table with compress=None 
Drop table if exists uao_tab_compress_none cascade;
CREATE TABLE uao_tab_compress_none (      
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=NONE ) ; 
insert into uao_tab_compress_none select i , 'This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling '||i, (random() * 10000000)::numeric + 10000000 from GENERATE_SERIES(10000, 19999) AS i;

CREATE index col_int_bmp_idx_compress_none on uao_tab_compress_none using bitmap (col_int);

\d+ uao_tab_compress_none
select count(*) from uao_tab_compress_none;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_none');

-- create select uao table with compress=zlib COMPRESSLEVEL=1 

Drop table if exists uao_tab_compress_zlib1 cascade;
CREATE TABLE uao_tab_compress_zlib1 (
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=zlib, COMPRESSLEVEL=1  ); 

insert into uao_tab_compress_zlib1 select i , 'This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling '||i, (random() * 10000000)::numeric + 10000000 from GENERATE_SERIES(10000, 19999) AS i;

CREATE index col_int_bmp_idx_compress_zlib1 on uao_tab_compress_zlib1 using bitmap (col_int);

\d+ uao_tab_compress_zlib1
select count(*) from uao_tab_compress_zlib1;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_zlib1');
SELECT 1 AS compression_present from pg_appendonly WHERE compresstype='zlib' AND compresslevel=1 AND relid=(SELECT oid  FROM pg_class WHERE relname='uao_tab_compress_zlib1');
-- create select uao table with compress=zlib COMPRESSLEVEL=9 

Drop table if exists uao_tab_compress_zlib9 cascade;
CREATE TABLE uao_tab_compress_zlib9 (
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=zlib, COMPRESSLEVEL=9  );   

insert into uao_tab_compress_zlib9 select i , 'This is news of today: Deadlock between Republicans and Democrats over how best to reduce the U.S. deficit, and over what period, has blocked an agreement to allow the raising of the $14.3 trillion debt ceiling '||i, (random() * 10000000)::numeric + 10000000 from GENERATE_SERIES(10000, 19999) AS i;

CREATE index col_int_bmp_idx_compress_zlib9 on uao_tab_compress_zlib9 using bitmap (col_int);

\d+ uao_tab_compress_zlib9
select count(*) from uao_tab_compress_zlib9;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_zlib9');

SELECT 1 AS compression_present from pg_appendonly WHERE compresstype='zlib' AND compresslevel=9 AND relid=(SELECT oid  FROM pg_class WHERE relname='uao_tab_compress_zlib9');
