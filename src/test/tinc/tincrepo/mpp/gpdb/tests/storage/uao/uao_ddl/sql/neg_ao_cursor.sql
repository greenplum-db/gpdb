--------------------------------------------------------------------------
-- @TITLE: Negative tests for UAO
-- 
--------------------------------------------------------------------------

Drop table if exists sto_ao_neg_cursor;
CREATE TABLE sto_ao_neg_cursor (id int, date date, amt decimal(10,2))
with (appendonly=true)
DISTRIBUTED BY (id);

Insert into sto_ao_neg_cursor values(1,'2013-07-05',19.20);
Insert into sto_ao_neg_cursor values(2,'2013-08-15',10.20);
Insert into sto_ao_neg_cursor values(3,'2013-09-09',14.20);
Insert into sto_ao_neg_cursor values(10,'2013-07-22',12.52);

begin;
DECLARE update_cursor CURSOR
FOR select * from sto_ao_neg_cursor ORDER BY id;

fetch 1 from update_cursor;

UPDATE sto_ao_neg_cursor SET amt = 40.40 WHERE CURRENT OF update_cursor;

end;

select * from sto_ao_neg_cursor ORDER BY id;


