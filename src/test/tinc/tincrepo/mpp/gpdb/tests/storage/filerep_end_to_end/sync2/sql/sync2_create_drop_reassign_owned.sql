-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
--
-- SYNC2
--
CREATE ROLE sync2_user1;


CREATE ROLE sync2_admin;

CREATE TABLE sync2_foo1 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync2_foo1 OWNER TO sync2_user1;


--
-- DROP OWNED 
--
DROP OWNED by  sync1_user7;
DROP OWNED by  ck_sync1_user6;
DROP OWNED by  ct_user4;
DROP OWNED by  resync_user2;
DROP OWNED by sync2_user1;


CREATE TABLE sync2_foo11 (i int, j int) DISTRIBUTED  RANDOMLY;
ALTER TABLE sync2_foo11 OWNER TO sync2_user1;


--
-- REASSIGN OWNED
--
REASSIGN OWNED BY sync1_user7 to sync2_admin;
REASSIGN OWNED BY ck_sync1_user6 to sync2_admin;
REASSIGN OWNED BY ct_user4 to sync2_admin;
REASSIGN OWNED BY resync_user2 to sync2_admin;
REASSIGN OWNED BY  sync2_user1 to sync2_admin;
