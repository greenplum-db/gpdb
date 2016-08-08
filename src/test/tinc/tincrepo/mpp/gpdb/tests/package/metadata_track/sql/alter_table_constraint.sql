--ADD table_constraint

          CREATE TABLE mdt_distributors (
          did integer,
          name varchar(40)
          ) DISTRIBUTED BY (name);

          ALTER TABLE mdt_distributors ADD UNIQUE(name);

--DROP CONSTRAINT constraint_name [ RESTRICT | CASCADE ]

          CREATE TABLE mdt_films (
          code char(5),
          title varchar(40),
          did integer,
          date_prod date,
          kind varchar(10),
          len interval hour to minute,
          CONSTRAINT production UNIQUE(date_prod)
          );

          ALTER TABLE mdt_films DROP CONSTRAINT production RESTRICT;

          CREATE TABLE mdt_films1 (
          code char(5),
          title varchar(40),
          did integer,
          date_prod date,
          kind varchar(10),
          len interval hour to minute,
          CONSTRAINT production UNIQUE(date_prod)
          );

          ALTER TABLE mdt_films1 DROP CONSTRAINT production CASCADE;

drop table mdt_distributors;
drop table mdt_films;
drop table mdt_films1;

