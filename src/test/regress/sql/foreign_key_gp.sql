--
-- GPDB currently does not support foreign key constraints: table can be created
-- with the constraints, but they won't be enforced. Tables operations will ignore
-- the constraints. We should not see errors.
--
CREATE TABLE fk_ref(id int primary key);
CREATE TABLE fk_heap(id int references fk_ref);
CREATE TABLE fk_ao(id int references fk_ref) USING ao_row;
CREATE TABLE fk_co(id int references fk_ref) USING ao_column;
INSERT INTO fk_heap VALUES (1);
INSERT INTO fk_ao VALUES (1);
INSERT INTO fk_co VALUES (1);

