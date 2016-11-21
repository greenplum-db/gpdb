-- @description : delete from UAO tables , with various compression type
-- 


-- create select uao table with compress=None 
Drop table if exists uao_tab_compress_none_del cascade;
CREATE TABLE uao_tab_compress_none_del (      
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=NONE ) ; 

\d+ uao_tab_compress_none_del
select count(*) from uao_tab_compress_none_del;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_none_del');


insert into uao_tab_compress_none_del values(1,'val1',100);
insert into uao_tab_compress_none_del values(2,'val2',200);
insert into uao_tab_compress_none_del values(3,'val3',300);
select *  from uao_tab_compress_none_del order by col_int;
delete from uao_tab_compress_none_del  where col_int = 1;
select *  from uao_tab_compress_none_del order by col_int;
select count(*) AS only_visi_tups  from uao_tab_compress_none_del ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_compress_none_del ;
select *  from uao_tab_compress_none_del order by col_int;

set gp_select_invisible = false;

-- create select uao table with compress=zlib COMPRESSLEVEL=1 

Drop table if exists uao_tab_compress_zlib1_del cascade;
CREATE TABLE uao_tab_compress_zlib1_del (
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=zlib, COMPRESSLEVEL=1  ); 

\d+ uao_tab_compress_zlib1_del
select count(*) from uao_tab_compress_zlib1_del;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_zlib1_del');
SELECT 1 AS compression_present from pg_appendonly WHERE compresstype='zlib' AND compresslevel=1 AND relid=(SELECT oid  FROM pg_class WHERE relname='uao_tab_compress_zlib1_del');

insert into uao_tab_compress_zlib1_del values(1,'val1',100);
insert into uao_tab_compress_zlib1_del values(2,'val2',200);
insert into uao_tab_compress_zlib1_del values(3,'val3',300);
select *  from uao_tab_compress_zlib1_del order by col_int;
delete from uao_tab_compress_zlib1_del  where col_int = 1;
select *  from uao_tab_compress_zlib1_del order by col_int;
select count(*) AS only_visi_tups  from uao_tab_compress_zlib1_del ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_compress_zlib1_del ;
select *  from uao_tab_compress_zlib1_del order by col_int;

set gp_select_invisible = false;

-- create select uao table with compress=zlib COMPRESSLEVEL=9 

Drop table if exists uao_tab_compress_zlib9_del cascade;
CREATE TABLE uao_tab_compress_zlib9_del (
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true , COMPRESSTYPE=zlib, COMPRESSLEVEL=9  );   

\d+ uao_tab_compress_zlib9_del
select count(*) from uao_tab_compress_zlib9_del;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_zlib9_del');

SELECT 1 AS compression_present from pg_appendonly WHERE compresstype='zlib' AND compresslevel=9 AND relid=(SELECT oid  FROM pg_class WHERE relname='uao_tab_compress_zlib9_del');

insert into uao_tab_compress_zlib9_del values(1,'val1',100);
insert into uao_tab_compress_zlib9_del values(2,'val2',200);
insert into uao_tab_compress_zlib9_del values(3,'val3',300);
select *  from uao_tab_compress_zlib9_del order by col_int;
delete from uao_tab_compress_zlib9_del  where col_int = 1;
select *  from uao_tab_compress_zlib9_del order by col_int;
select count(*) AS only_visi_tups  from uao_tab_compress_zlib9_del ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_compress_zlib9_del ;
select *  from uao_tab_compress_zlib9_del order by col_int;

set gp_select_invisible = false;
