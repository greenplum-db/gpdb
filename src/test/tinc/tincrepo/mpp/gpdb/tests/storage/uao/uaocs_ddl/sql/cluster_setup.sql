-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
DROP TABLE IF EXISTS ao;

CREATE TABLE ao (a INT) WITH (appendonly=true, orientation=column);
CREATE INDEX ao_index ON ao(a);
insert into ao select generate_series(1,10);

