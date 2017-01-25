-- @gucs gp_create_table_random_default_distribution=off
-- Create heap tables 

CREATE TABLE heap_table1(
          col_text text,
          col_numeric numeric
          ) DISTRIBUTED RANDOMLY;

insert into heap_table1 values ('0_zero',0);
insert into heap_table1 values ('1_one',1);
insert into heap_table1 values ('2_two',2);
