-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE DOMAIN resync_domain_1 AS int DEFAULT 1 CONSTRAINT cons_not_null NOT NULL;
CREATE DOMAIN resync_domain_2 AS int CONSTRAINT cons_null NULL;
CREATE DOMAIN resync_domain_3 AS TEXT ;

DROP DOMAIN sync1_domain_6;
DROP DOMAIN ck_sync1_domain_5;
DROP DOMAIN ct_domain_3;
DROP DOMAIN resync_domain_1;
