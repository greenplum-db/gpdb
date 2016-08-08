--Inherits parent_table - MPP-5300

   CREATE TABLE mdt_table_parent ( a int, b text) DISTRIBUTED BY (a);

   CREATE TABLE mdt_table_child() INHERITS(mdt_table_parent);

drop table mdt_table_child;
drop table mdt_table_parent ;

