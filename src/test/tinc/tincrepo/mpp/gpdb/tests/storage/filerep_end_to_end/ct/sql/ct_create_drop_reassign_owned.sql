-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- CT
--
CREATE ROLE ct_user1;
CREATE ROLE ct_user2;
CREATE ROLE ct_user3;
CREATE ROLE ct_user4;



CREATE ROLE ct_admin;

CREATE TABLE ct_foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ct_foo1 OWNER TO ct_user1;
CREATE TABLE ct_foo2 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ct_foo2 OWNER TO ct_user2;
CREATE TABLE ct_foo3 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ct_foo3 OWNER TO ct_user3;
CREATE TABLE ct_foo4 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ct_foo4 OWNER TO ct_user4;


--
-- DROP OWNED 
--
DROP OWNED by  sync1_user4;
DROP OWNED by  ck_sync1_user3;
DROP OWNED by ct_user1;


CREATE TABLE ct_foo11 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ct_foo11 OWNER TO ct_user1;
CREATE TABLE ct_foo22(i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ct_foo22 OWNER TO ct_user2;
CREATE TABLE ct_foo33 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ct_foo33 OWNER TO ct_user3;
CREATE TABLE ct_foo44 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE ct_foo44 OWNER TO ct_user4;


--
-- REASSIGN OWNED
--
REASSIGN OWNED BY sync1_user4 to ct_admin;
REASSIGN OWNED BY ck_sync1_user3 to ct_admin;
REASSIGN OWNED BY  ct_user1 to ct_admin;
