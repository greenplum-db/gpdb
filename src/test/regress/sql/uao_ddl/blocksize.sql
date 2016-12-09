-- Insert/update/delete on UAO tables with different blocksizes
BEGIN;
CREATE TABLE uao_blocksize_8k with (appendonly=true, BLOCKSIZE=8192) AS (
SELECT GENERATE_SERIES::numeric sno
	 , (random() * 10000000)::numeric + 10000000 val1
	 , timeofday()::varchar(50) val2
	 , GENERATE_SERIES % 103 val3
	 , (random() * 10000000)::numeric + 10000000 val4
	 , (random() * 10000000)::numeric + 10000000 val5
  FROM GENERATE_SERIES(1000, 1999)
) distributed by (sno);

CREATE index val3_bmp_idxblocksize_upd_8k on uao_blocksize_8k
 using bitmap (val3);
\d+ uao_blocksize_8k
SELECT count(*) from uao_blocksize_8k;
SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is
not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM
pg_class WHERE relname='uao_blocksize_8k');

select 1 as block8k_present from pg_appendonly WHERE blocksize=8192
AND relid=(SELECT oid FROM pg_class WHERE
relname='uao_blocksize_8k');

update uao_blocksize_8k set val2 = 'new_val', val3 = -sno
 where val3 < 54;
select count(*) AS visible_tuples from uao_blocksize_8k;
select count(*) AS updated_tuples from uao_blocksize_8k
 where val2='new_val';
set gp_select_invisible = true;
select count(*) AS all_tuples from uao_blocksize_8k;
set gp_select_invisible = false;
delete from uao_blocksize_8k where val3 < 0;
select count(*) AS visible_tuples from uao_blocksize_8k;
set gp_select_invisible = true;
select count(*) AS all_tuples from uao_blocksize_8k;
set gp_select_invisible = false;

-- create select uao table with BLOCKSIZE=2048K
CREATE TABLE uao_blocksize_2048k
 with (appendonly=true, BLOCKSIZE=2097152) AS (
SELECT GENERATE_SERIES::numeric sno
	 , (random() * 10000000)::numeric + 10000000 val1
	 , timeofday()::varchar(50) val2
	 , GENERATE_SERIES % 103 val3
	 , (random() * 10000000)::numeric + 10000000 val4
	 , (random() * 10000000)::numeric + 10000000 val5
  FROM GENERATE_SERIES(1000, 1999)
) distributed by (sno);

SELECT count(*) from uao_blocksize_2048k;

SELECT 1 AS VisimapPresent FROM pg_appendonly WHERE visimapidxid is
not NULL AND visimapidxid is not NULL AND relid=(SELECT oid FROM
pg_class WHERE relname='uao_blocksize_2048k');

select 1 as block2048k_present from pg_appendonly WHERE
blocksize=2097152 AND relid=(SELECT oid FROM pg_class WHERE
relname='uao_blocksize_2048k');

update uao_blocksize_2048k set val2 = 'new_val', val3 = -sno
 where val3 < 54;
select count(*) AS visible_tuples from uao_blocksize_2048k;
select count(*) AS updated_tuples from uao_blocksize_2048k
 where val2='new_val';
set gp_select_invisible = true;
select count(*) AS all_tuples from uao_blocksize_2048k;
set gp_select_invisible = false;
delete from uao_blocksize_2048k where val3 < 0;
select count(*) AS visible_tuples from uao_blocksize_2048k;
set gp_select_invisible = true;
select count(*) AS all_tuples from uao_blocksize_2048k;
set gp_select_invisible = false;
COMMIT;
