-- @description : update UAO tables , with various compression type
-- 


-- create select uao table with compress=None 
Drop table if exists uao_tab_compress_none_upd cascade;
CREATE TABLE uao_tab_compress_none_upd (      
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true, orientation=column , COMPRESSTYPE=NONE ) ; 

CREATE index col_int_bmp_idx_compress_upd_none on uao_tab_compress_none_upd using bitmap (col_int);

\d+ uao_tab_compress_none_upd
select count(*) from uao_tab_compress_none_upd;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_none_upd');


insert into uao_tab_compress_none_upd values(1,'val1',100);
insert into uao_tab_compress_none_upd values(2,'val2',200);
insert into uao_tab_compress_none_upd values(3,'val3',300);
select *  from uao_tab_compress_none_upd order by col_int,col_text;
update uao_tab_compress_none_upd set col_text=col_text||' new value' where col_int = 1;
select *  from uao_tab_compress_none_upd order by col_int,col_text;
select count(*) AS only_visi_tups  from uao_tab_compress_none_upd ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_compress_none_upd ;
select *  from uao_tab_compress_none_upd order by col_int,col_text;

set gp_select_invisible = false;

-- create select uao table with compress=zlib COMPRESSLEVEL=1 

Drop table if exists uao_tab_compress_zlib1_upd cascade;
CREATE TABLE uao_tab_compress_zlib1_upd (
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true, orientation=column , COMPRESSTYPE=zlib, COMPRESSLEVEL=1  ); 


CREATE index col_int_bmp_idx_compress_upd_zlib1 on uao_tab_compress_zlib1_upd using bitmap (col_int);

\d+ uao_tab_compress_zlib1_upd
select count(*) from uao_tab_compress_zlib1_upd;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_zlib1_upd');
SELECT 1 AS compression_present from pg_appendonly WHERE compresstype='zlib' AND compresslevel=1 AND relid=(SELECT oid  FROM pg_class WHERE relname='uao_tab_compress_zlib1_upd');

insert into uao_tab_compress_zlib1_upd values(1,'val1',100);
insert into uao_tab_compress_zlib1_upd values(2,'val2',200);
insert into uao_tab_compress_zlib1_upd values(3,'val3',300);
select *  from uao_tab_compress_zlib1_upd order by col_int, col_text;
update uao_tab_compress_zlib1_upd set col_text=col_text||' new value' where col_int = 1;
select *  from uao_tab_compress_zlib1_upd order by col_int, col_text;
select count(*) AS only_visi_tups  from uao_tab_compress_zlib1_upd ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_compress_zlib1_upd ;
select *  from uao_tab_compress_zlib1_upd order by col_int,col_text;

set gp_select_invisible = false;

-- create select uao table with compress=zlib COMPRESSLEVEL=9 

Drop table if exists uao_tab_compress_zlib9_upd cascade;
CREATE TABLE uao_tab_compress_zlib9_upd (
          col_int int,
          col_text text,
          col_numeric numeric)
 with (appendonly=true, orientation=column , COMPRESSTYPE=zlib, COMPRESSLEVEL=9  );   


CREATE index col_int_bmp_idx_compress_upd_zlib9 on uao_tab_compress_zlib9_upd using bitmap (col_int);

\d+ uao_tab_compress_zlib9_upd
select count(*) from uao_tab_compress_zlib9_upd;

SELECT 1 AS VisimapPresent  FROM pg_appendonly WHERE visimapidxid is not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM pg_class WHERE relname='uao_tab_compress_zlib9_upd');

SELECT 1 AS compression_present from pg_appendonly WHERE compresstype='zlib' AND compresslevel=9 AND relid=(SELECT oid  FROM pg_class WHERE relname='uao_tab_compress_zlib9_upd');

insert into uao_tab_compress_zlib9_upd values(1,'val1',100);
insert into uao_tab_compress_zlib9_upd values(2,'val2',200);
insert into uao_tab_compress_zlib9_upd values(3,'val3',300);
select *  from uao_tab_compress_zlib9_upd order by col_int,col_text;
update uao_tab_compress_zlib9_upd set col_text=col_text||' new value' where col_int = 1;
select *  from uao_tab_compress_zlib9_upd order by col_int,col_text;
select count(*) AS only_visi_tups  from uao_tab_compress_zlib9_upd ;
set gp_select_invisible = true;
select count(*) AS invisi_and_visi_tups  from uao_tab_compress_zlib9_upd ;
select *  from uao_tab_compress_zlib9_upd order by col_int,col_text;

set gp_select_invisible = false;

