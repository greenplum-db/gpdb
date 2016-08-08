-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
CREATE DOMAIN ct_domain_1 AS int DEFAULT 1 CONSTRAINT cons_not_null NOT NULL;
CREATE DOMAIN ct_domain_2 AS int CONSTRAINT cons_null NULL;
CREATE DOMAIN ct_domain_3 AS TEXT ;
CREATE DOMAIN ct_domain_4 AS TEXT CHECK ( VALUE ~ E'\\d{5}$' OR VALUE ~ E'\\d{5}-\\d{4}$');
CREATE DOMAIN ct_domain_5 AS TEXT CHECK ( VALUE ~ E'\\d{5}$' OR VALUE ~ E'\\d{5}-\\d{4}$');

DROP DOMAIN sync1_domain_4;
DROP DOMAIN ck_sync1_domain_3;
DROP DOMAIN ct_domain_1;
