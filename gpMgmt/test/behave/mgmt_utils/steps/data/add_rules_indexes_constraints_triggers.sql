-- create various rules, constraints, indexes, domains

DROP TABLE IF EXISTS mytable;
DROP TABLE IF EXISTS mytable2;
DROP TABLE IF EXISTS mytable3;
DROP TABLE IF EXISTS mytable4;
DROP TABLE IF EXISTS us_snail_addy;
DROP DOMAIN IF EXISTS us_postal_code;
CREATE TABLE mytable (i int, s varchar);
CREATE TABLE mytable2 (i int);
CREATE TABLE mytable3 (i int PRIMARY KEY);
CREATE TABLE mytable4 (i int UNIQUE);
CREATE TABLE mytable5 (id int, date date PRIMARY KEY, amt decimal(10,2))
DISTRIBUTED BY (date)
PARTITION BY RANGE (date)
( START (date '2018-01-01') INCLUSIVE
   END (date '2018-01-31') EXCLUSIVE
   EVERY (INTERVAL '1 week') );


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

CREATE OR REPLACE FUNCTION do_nothing () RETURNS TRIGGER AS
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
ALTER TABLE mytable2 ADD CONSTRAINT unique_constraint UNIQUE (i);
ALTER TABLE mytable3 ADD CONSTRAINT foreign_key FOREIGN KEY (i) REFERENCES mytable4 (i) MATCH FULL;
