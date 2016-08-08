
--------------------------------------------------------------------------
-- @TITLE: Negative tests for UAO
-- 
--------------------------------------------------------------------------


Drop table if exists sto_ao_neg_index;
CREATE TABLE sto_ao_neg_index (id int, date date, amt decimal(10,2))
with (appendonly=true)
DISTRIBUTED BY (id);

drop index if exists uni_index;
CREATE UNIQUE INDEX uni_index ON sto_ao_neg_index (date);

drop index if exists clustered_index;
CREATE INDEX clustered_index ON sto_ao_neg_index (date);
CLUSTER clustered_index ON sto_ao_neg_index;


