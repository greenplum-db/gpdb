SHOW server_version;

--
-- create partitioned ao table
--
CREATE TABLE p_ao_table (id integer, name text) WITH (appendonly=true) DISTRIBUTED BY (id) PARTITION BY RANGE(id) (START(1) END(3) EVERY(1));
INSERT INTO p_ao_table VALUES (1, 'Jane');
INSERT INTO p_ao_table VALUES (2, 'John');

--
-- create partitioned ao table with multiple segfiles
--
CREATE TABLE p_ao_table_with_multiple_segfiles (id int, name text) WITH (appendonly=true) DISTRIBUTED BY (id)
PARTITION BY RANGE (id)
    SUBPARTITION BY LIST (name)
        SUBPARTITION TEMPLATE (
         SUBPARTITION jane VALUES ('Jane'),
          SUBPARTITION john VALUES ('John'),
           DEFAULT SUBPARTITION other_names )
(START (1) END (2) EVERY (1),
    DEFAULT PARTITION other_ids);

-- use multiple sessions to create multiple segfiles
1:CREATE INDEX p_ao_name_index ON p_ao_table_with_multiple_segfiles(name);
1:BEGIN;
1:INSERT INTO p_ao_table_with_multiple_segfiles VALUES (1, 'Jane');
1:INSERT INTO p_ao_table_with_multiple_segfiles VALUES (2, 'Jane');

2:BEGIN;
2:INSERT INTO p_ao_table_with_multiple_segfiles VALUES (1, 'Jane');
2:INSERT INTO p_ao_table_with_multiple_segfiles VALUES (2, 'Jane');
2:INSERT INTO p_ao_table_with_multiple_segfiles VALUES (4, 'Andy');

1:END;
2:END;

-- ensure that we can correctly upgrade tables with dropped or deleted tuples
UPDATE p_ao_table_with_multiple_segfiles SET name='Carolyn' WHERE name='Andy';
INSERT INTO p_ao_table_with_multiple_segfiles VALUES (5, 'Bob');
DELETE FROM p_ao_table_with_multiple_segfiles WHERE id=5;

-- current customer workaround to upgrade tables with indexes is to drop and recreate.
DROP INDEX p_ao_name_index;
DROP INDEX p_ao_name_index_1_prt_2;
DROP INDEX p_ao_name_index_1_prt_other_ids;
DROP INDEX p_ao_name_index_1_prt_2_2_prt_jane;
DROP INDEX p_ao_name_index_1_prt_2_2_prt_john;
DROP INDEX p_ao_name_index_1_prt_2_2_prt_other_names;
DROP INDEX p_ao_name_index_1_prt_other_ids_2_prt_jane;
DROP INDEX p_ao_name_index_1_prt_other_ids_2_prt_john;
DROP INDEX p_ao_name_index_1_prt_other_ids_2_prt_other_names;
