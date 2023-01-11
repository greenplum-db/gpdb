--
-- Extra GPDB tests on INSERT/UPDATE/DELETE RETURNING
--

CREATE TABLE returning_parttab (distkey int4, partkey int4, i int, t text)
DISTRIBUTED BY (distkey)
PARTITION BY RANGE (partkey) (START (1) END (10));

--
-- Test INSERT RETURNING with partitioning
--
insert into returning_parttab values (1, 1, 1, 'single insert') returning *;
insert into returning_parttab
select 1, g, g, 'multi ' || g from generate_series(1, 5) g
returning distkey, partkey, i, t;

-- Drop a column, and create a new partition. The new partition will not have
-- the dropped column, while in the old partition, it's still physically there,
-- just marked as dropped. Make sure the executor maps the columns correctly.
ALTER TABLE returning_parttab DROP COLUMN i;

alter table returning_parttab add partition newpart start (10) end (20);

insert into returning_parttab values (1, 10, 'single2 insert') returning *;
insert into returning_parttab select 2, g + 10, 'multi2 ' || g from generate_series(1, 5) g
returning distkey, partkey, t;

--
-- Test UPDATE/DELETE RETURNING with partitioning
--
update returning_parttab set partkey = 9 where partkey = 3 returning *;
update returning_parttab set partkey = 19 where partkey = 13 returning *;

-- update that moves the tuple across partitions (not supported)
update returning_parttab set partkey = 18 where partkey = 4 returning *;

-- delete
delete from returning_parttab where partkey = 14 returning *;


-- Check table contents, to be sure that all the commands did what they claimed.
select * from returning_parttab;

--
-- DELETE RETURNING is currently not supported on AO tables.
--
CREATE TEMP TABLE returning_aotab (id int4) WITH (appendonly=true);
INSERT INTO returning_aotab VALUES (1);
DELETE FROM returning_aotab RETURNING *;


--
-- Test UPDATE RETURNING with a split update, i.e. an update of the distribution
-- key.
--
CREATE TEMP TABLE returning_disttest (id int4) DISTRIBUTED BY (id);
INSERT INTO returning_disttest VALUES (1), (2);

-- Disable QUIET mode, so that we get some testing of the command tag as well.
-- (At one point, each split update incorrectly counted as two updated rows.)
\set QUIET off

UPDATE returning_disttest SET id = id + 1;

SELECT * FROM returning_disttest;

--
-- Test returning ctid with trigger
--
CREATE TABLE returning_ctid (f1 serial, f2 text) DISTRIBUTED BY (f1);
-- Create function used by trigger
CREATE FUNCTION trig_row_before_insupdate() RETURNS TRIGGER AS $$
  BEGIN
    NEW.f2 := NEW.f2 || ' triggered !';
    RETURN NEW;
  END
$$ language plpgsql;
-- Create trigger for each row insert or update
CREATE TRIGGER trig_row_before BEFORE INSERT OR UPDATE ON returning_ctid
FOR EACH ROW EXECUTE PROCEDURE trig_row_before_insupdate();

-- Check returning sys attribute on insert 
INSERT INTO returning_ctid(f2) VALUES ('test') RETURNING ctid;
SELECT *, ctid FROM returning_ctid;

-- Clean up
DROP TRIGGER trig_row_before ON returning_ctid;
DROP FUNCTION trig_row_before_insupdate() CASCADE;
DROP TABLE returning_ctid;

--
-- Test returning ctid with trigger for AOCO table
--
CREATE TABLE returning_ctid_aoco (f1 serial, f2 text) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (f1);
-- Create function used by trigger
CREATE FUNCTION trig_row_before_insupdate() RETURNS TRIGGER AS $$
  BEGIN
    NEW.f2 := NEW.f2 || ' triggered !';
    RETURN NEW;
  END
$$ language plpgsql;
-- Create trigger for each row insert
CREATE TRIGGER trig_row_before BEFORE INSERT ON returning_ctid_aoco
FOR EACH ROW EXECUTE PROCEDURE trig_row_before_insupdate();

-- Check returning sys attribute on insert
INSERT INTO returning_ctid_aoco(f2) VALUES ('test') RETURNING ctid;
SELECT *, ctid FROM returning_ctid_aoco;

-- Clean up
DROP TRIGGER trig_row_before ON returning_ctid_aoco;
DROP FUNCTION trig_row_before_insupdate() CASCADE;
DROP TABLE returning_ctid_aoco;
