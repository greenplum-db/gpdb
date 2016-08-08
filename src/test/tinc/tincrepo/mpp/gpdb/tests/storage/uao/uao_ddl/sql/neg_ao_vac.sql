--------------------------------------------------------------------------
-- @TITLE: Negative tests for UAO
-- 
--------------------------------------------------------------------------

Drop table if exists sto_ao_neg_vac;
CREATE TABLE sto_ao_neg_vac (id int, date date, amt decimal(10,2))
with (appendonly=true)
DISTRIBUTED BY (id);

Insert into sto_ao_neg_vac values(1,'2013-07-05',19.20);
Insert into sto_ao_neg_vac values(2,'2013-08-15',10.20);
Insert into sto_ao_neg_vac values(3,'2013-09-09',14.20);
Insert into sto_ao_neg_vac values(10,'2013-07-22',12.52);

select * from sto_ao_neg_vac ORDER BY id;

UPDATE sto_ao_neg_vac SET amt = 40.40 WHERE id =2;

select * from sto_ao_neg_vac ORDER BY id,amt;

set gp_select_invisible = true;

select * from sto_ao_neg_vac ORDER BY id,amt;

set gp_appendonly_compaction=false;

select * from sto_ao_neg_vac ORDER BY id,amt;

VACUUM ANALYZE sto_ao_neg_vac;

select * from sto_ao_neg_vac ORDER BY id,amt;


set gp_appendonly_compaction=true;

select * from sto_ao_neg_vac ORDER BY id,amt;

VACUUM ANALYZE sto_ao_neg_vac;

select * from sto_ao_neg_vac ORDER BY id,amt;


