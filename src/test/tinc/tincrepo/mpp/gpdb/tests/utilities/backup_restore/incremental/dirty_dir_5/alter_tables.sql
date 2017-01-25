-- @gucs gp_create_table_random_default_distribution=off
-- Add partitions
Alter table ao_part13 add partition new_p1 start(5) end(7);
Alter table ao_part13 add partition new_p2 start(7) end(9);
Alter table ao_part13 add partition new_p3 start(9) end(11);
Alter table ao_part13 add partition new_p4 start(11) end(13);

Insert into ao_part13 values(1,generate_series(5,12),generate_series(1,6),4);

