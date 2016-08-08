CREATE ROLE mdt_sally;
CREATE ROLE mdt_ron;
CREATE ROLE mdt_ken;
CREATE ROLE mdt_admin;

CREATE TABLE mdt_foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE mdt_foo1 OWNER TO mdt_sally;
CREATE TABLE mdt_foo2 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE mdt_foo2 OWNER TO mdt_ron;
CREATE TABLE mdt_foo3 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE mdt_foo3 OWNER TO mdt_ken;

REASSIGN OWNED BY mdt_sally,mdt_ron,mdt_ken to mdt_admin;

drop table mdt_foo1;
drop table mdt_foo2;
drop table mdt_foo3;
DROP ROLE mdt_sally;
DROP ROLE mdt_ron;
DROP ROLE mdt_ken;
DROP ROLE mdt_admin;
