-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC1
--

CREATE ROLE sync1_user1;
CREATE ROLE sync1_user2;
CREATE ROLE sync1_user3;
CREATE ROLE sync1_user4;
CREATE ROLE sync1_user5;
CREATE ROLE sync1_user6;
CREATE ROLE sync1_user7;

CREATE ROLE sync1_admin;

CREATE TABLE sync1_foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo1 OWNER TO sync1_user1;
CREATE TABLE sync1_foo2 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo2 OWNER TO sync1_user2;
CREATE TABLE sync1_foo3 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo3 OWNER TO sync1_user3;
CREATE TABLE sync1_foo4 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo4 OWNER TO sync1_user4;
CREATE TABLE sync1_foo5 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo5 OWNER TO sync1_user5;
CREATE TABLE sync1_foo6 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo6 OWNER TO sync1_user6;
CREATE TABLE sync1_foo7 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo7 OWNER TO sync1_user7;

DROP OWNED by sync1_user1;


CREATE TABLE sync1_foo11 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo11 OWNER TO sync1_user1;
CREATE TABLE sync1_foo22(i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo22 OWNER TO sync1_user2;
CREATE TABLE sync1_foo33 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo33 OWNER TO sync1_user3;
CREATE TABLE sync1_foo44 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo44 OWNER TO sync1_user4;
CREATE TABLE sync1_foo55 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo55 OWNER TO sync1_user5;
CREATE TABLE sync1_foo66 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo66 OWNER TO sync1_user6;
CREATE TABLE sync1_foo77 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync1_foo77 OWNER TO sync1_user7;
REASSIGN OWNED BY  sync1_user1 to sync1_admin;
