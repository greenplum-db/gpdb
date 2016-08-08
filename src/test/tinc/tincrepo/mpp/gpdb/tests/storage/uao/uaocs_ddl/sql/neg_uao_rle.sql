--------------------------------------------------------------------------
-- @TITLE: Negative tests for UAO
-- 
--------------------------------------------------------------------------

-- UAO row orientation table with compresstype = RLE_TYPE is not supported
Drop table if exists sto_uao_neg_RLE;
CREATE TABLE sto_uao_neg_RLE (id int, date date, amt decimal(10,2))
with (appendonly=true, COMPRESSTYPE=RLE_TYPE);

