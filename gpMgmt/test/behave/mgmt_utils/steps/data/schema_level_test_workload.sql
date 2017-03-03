DROP DATABASE IF EXISTS schema_level_test_db;

CREATE DATABASE schema_level_test_db;

\c schema_level_test_db

DROP SCHEMA IF EXISTS s1 CASCADE;
CREATE SCHEMA s1;

CREATE SEQUENCE s1.id_seq
    START WITH 1
    INCREMENT BY 1;

CREATE TABLE s1.apples (
    id integer DEFAULT nextval('s1.id_seq'::regclass) NOT NULL,
    text text);

CREATE VIEW s1.v1 AS SELECT * FROM s1.apples;

INSERT INTO s1.apples VALUES ( default, 'green');
INSERT INTO s1.apples VALUES ( default, 'red');

CREATE OR REPLACE FUNCTION s1.increment(i integer) RETURNS integer AS $$
        BEGIN
                RETURN i + 1;
        END;
$$ LANGUAGE plpgsql;
