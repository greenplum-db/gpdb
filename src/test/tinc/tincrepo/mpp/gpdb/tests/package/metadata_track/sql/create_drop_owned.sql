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

drop OWNED by mdt_sally CASCADE;
drop OWNED by mdt_ron RESTRICT;
drop OWNED by mdt_ken;
drop ROLE mdt_sally;
drop ROLE mdt_ron;
drop ROLE mdt_ken;
drop ROLE mdt_admin;
