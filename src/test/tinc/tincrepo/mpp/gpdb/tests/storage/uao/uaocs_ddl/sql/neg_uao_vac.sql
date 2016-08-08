--------------------------------------------------------------------------
-- @TITLE: Negative tests for UAO
-- 
--------------------------------------------------------------------------

Drop table if exists sto_uao_neg_vac;
CREATE TABLE sto_uao_neg_vac (id int, date date, amt decimal(10,2))
with (appendonly=true, orientation=column)
DISTRIBUTED BY (id);

Insert into sto_uao_neg_vac values(1,'2013-07-05',19.20);
Insert into sto_uao_neg_vac values(2,'2013-08-15',10.20);
Insert into sto_uao_neg_vac values(3,'2013-09-09',14.20);
Insert into sto_uao_neg_vac values(10,'2013-07-22',12.52);

select * from sto_uao_neg_vac ORDER BY id;

UPDATE sto_uao_neg_vac SET amt = 40.40 WHERE id =2;

--if gp_select_invisible = falase, hidden tuples in append-only tables are not returned by SELECT statements.
select * from sto_uao_neg_vac ORDER BY id,amt;

set gp_select_invisible = true;

select * from sto_uao_neg_vac ORDER BY id,amt;

--if gp_appendonly_compaction=false, segment files should not be compacted during VACUUM commands.
set gp_appendonly_compaction=false;

select * from sto_uao_neg_vac ORDER BY id,amt;

VACUUM ANALYZE sto_uao_neg_vac;

select * from sto_uao_neg_vac ORDER BY id,amt;


-- If the ratio of hidden tuples in a segment file on a segment is greater than threshold, the segment file is compacted on a non-full VACUUM call. [in this case threshold is 10 and 25% of the rows have changed]
set gp_appendonly_compaction=true;

select * from sto_uao_neg_vac ORDER BY id,amt;

VACUUM ANALYZE sto_uao_neg_vac;

select * from sto_uao_neg_vac ORDER BY id,amt;


