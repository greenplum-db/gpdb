--------------------------------------------------------------------------
-- @TITLE: Negative tests for UAO
-- 
--------------------------------------------------------------------------

Drop table if exists sto_ao_neg_RLE;
CREATE TABLE sto_ao_neg_RLE (id int, date date, amt decimal(10,2))
with (appendonly=true, COMPRESSTYPE=RLE_TYPE);

