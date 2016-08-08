-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CK_SYNC1
--
CREATE ROLE ck_sync1_user1;
CREATE ROLE ck_sync1_user2;
CREATE ROLE ck_sync1_user3;
CREATE ROLE ck_sync1_user4;
CREATE ROLE ck_sync1_user5;
CREATE ROLE ck_sync1_user6;


CREATE ROLE ck_sync1_admin;

CREATE TABLE ck_sync1_foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo1 OWNER TO ck_sync1_user1;
CREATE TABLE ck_sync1_foo2 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo2 OWNER TO ck_sync1_user2;
CREATE TABLE ck_sync1_foo3 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo3 OWNER TO ck_sync1_user3;
CREATE TABLE ck_sync1_foo4 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo4 OWNER TO ck_sync1_user4;
CREATE TABLE ck_sync1_foo5 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo5 OWNER TO ck_sync1_user5;
CREATE TABLE ck_sync1_foo6 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo6 OWNER TO ck_sync1_user6;
--
-- DROP OWNED 
--
DROP OWNED by  sync1_user2;
DROP OWNED by ck_sync1_user1;


CREATE TABLE ck_sync1_foo11 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo11 OWNER TO ck_sync1_user1;
CREATE TABLE ck_sync1_foo22(i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo22 OWNER TO ck_sync1_user2;
CREATE TABLE ck_sync1_foo33 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo33 OWNER TO ck_sync1_user3;
CREATE TABLE ck_sync1_foo44 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo44 OWNER TO ck_sync1_user4;
CREATE TABLE ck_sync1_foo55 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo55 OWNER TO ck_sync1_user5;
CREATE TABLE ck_sync1_foo66 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ck_sync1_foo66 OWNER TO ck_sync1_user6;

--
-- REASSIGN OWNED
--
REASSIGN OWNED BY sync1_user2 to ck_sync1_admin;
REASSIGN OWNED BY  ck_sync1_user1 to ck_sync1_admin;
