-- Replace this with a dump of ICW once it works with gpexpand

SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET escape_string_warning = 'off';



CREATE TABLE test (
    name text,
    age int
);

INSERT INTO test (name, age)
    VALUES ('a', 1), ('b', 2);
