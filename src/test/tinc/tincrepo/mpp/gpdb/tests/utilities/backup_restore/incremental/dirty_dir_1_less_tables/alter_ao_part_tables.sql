-- @gucs gp_create_table_random_default_distribution=off
-- Alter table rename default partition
Alter table ao_part01 rename default partition to new_others;

-- Alter table rename partition
Alter table ao_part02 RENAME PARTITION FOR ('2008-01-01') TO jan08;

