-- create various rules, constraints, indexes, domains

drop table if exists mytable;
drop table if exists mytable2;
drop table if exists mytable3;
drop table if exists mytable4;
drop table if exists us_snail_addy;
drop domain if exists us_postal_code;
create table mytable (i int, s varchar);
create table mytable2 (i int);
create table mytable3 (i int PRIMARY KEY);
create table mytable4 (i int UNIQUE);


CREATE DOMAIN us_postal_code AS TEXT
CHECK(
   VALUE ~ '^\\d{5}$'
);

CREATE TABLE us_snail_addy (
  s varchar,
  address_id SERIAL,
  postal us_postal_code NOT NULL
);


CREATE OR REPLACE RULE myrule AS
  ON UPDATE TO mytable             -- another similar rule for DELETE
  DO INSTEAD NOTHING;

create or replace function do_nothing () returns trigger as
$$
begin
return new;
end;
$$ language plpgsql;



CREATE TRIGGER mytrigger
    BEFORE UPDATE ON mytable
    EXECUTE PROCEDURE do_nothing();

CREATE UNIQUE INDEX my_unique_index ON mytable (i);

-- Constraints
ALTER TABLE mytable ADD CONSTRAINT check_constraint_no_domain CHECK (char_length(s) < 30);
ALTER TABLE us_snail_addy ADD CONSTRAINT check_constraint_with_domain CHECK (char_length(s) < 30);
ALTER TABLE us_snail_addy ADD PRIMARY KEY (address_id);
ALTER TABLE mytable2 add CONSTRAINT unique_constraint UNIQUE (i);
ALTER TABLE mytable3 ADD CONSTRAINT foreign_key FOREIGN KEY (i) REFERENCES mytable4 (i) MATCH FULL;
