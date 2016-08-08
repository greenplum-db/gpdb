
--------------------------------------------------------------------------
-- @TITLE: Negative tests for UAO
-- 
--------------------------------------------------------------------------


Drop table if exists sto_uao_neg_index;
CREATE TABLE sto_uao_neg_index (id int, date date, amt decimal(10,2))
with (appendonly=true, orientation=column)
DISTRIBUTED BY (id);

-- append-only tables do not support unique indexes
drop index if exists uni_index;
CREATE UNIQUE INDEX uni_index ON sto_uao_neg_index (date);

-- append only tables should not cluster
drop index if exists clustered_index;
CREATE INDEX clustered_index ON sto_uao_neg_index (date);
CLUSTER clustered_index ON sto_uao_neg_index;


