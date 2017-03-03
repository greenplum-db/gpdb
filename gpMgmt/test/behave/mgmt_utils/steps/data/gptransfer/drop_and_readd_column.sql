-- partition table
ALTER TABLE sales DROP COLUMN trans_id;
ALTER TABLE sales ADD COLUMN trans_id int;