-- Given a create table specifying a constraint
create table exclusion_constraints_t1(a int, b int, EXCLUDE (a WITH =));
-- Then we errored out

-- Given a create partition table table specifying a constraint
create table exclusion_constraints_pt1(a int, b int, EXCLUDE (b WITH =)) partition by range(a) (start(1) end(4) every(1));
-- Then we errored out

-- Given a table without constraints
create table exclusion_constraints_t2(a int, b int);
-- And we alter table to add constraints
ALTER TABLE exclusion_constraints_t2 ADD CONSTRAINT constraint_on_t2 EXCLUDE USING btree (b WITH =);
-- Then we errored out
