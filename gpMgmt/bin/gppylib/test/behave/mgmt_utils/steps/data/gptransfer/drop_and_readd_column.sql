-- partition table
ALTER TABLE employee DROP COLUMN gender;
ALTER TABLE employee ADD COLUMN gender char(1);