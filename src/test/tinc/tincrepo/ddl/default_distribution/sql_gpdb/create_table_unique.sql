-- UNIQUE default behavior
DROP TABLE IF EXISTS dupconstr;
create table dupconstr (
						i int,
						j int constraint test CHECK (j > 10),
						CONSTRAINT test1 UNIQUE (i,j));

-- UNIQUE default behavior
create unique index distpol_uidx on distpol(c1);

