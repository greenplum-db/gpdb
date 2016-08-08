-- @Description Tests that altering the storage type works as expected
-- 
set gp_setwith_alter_storage = true;
alter table atsdb set with(appendonly = true, compresslevel = 3);
SELECT i, j FROM atsdb;
DELETE FROM atsdb WHERE j = 1;
UPDATE atsdb SET i=1 WHERE j=2;
SELECT i, j FROM atsdb;
