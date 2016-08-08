--local table

    CREATE LOCAL TEMP TABLE mdt_table_local (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) DISTRIBUTED RANDOMLY;

--like parent_table

    CREATE TABLE mdt_table_like_parent (
    like mdt_table_local
    ) DISTRIBUTED RANDOMLY;

--like parent_table

    CREATE TABLE mdt_table_like_parent1 (
    like mdt_table_local INCLUDING DEFAULTS
    ) DISTRIBUTED RANDOMLY;

--like parent_table

    CREATE TABLE mdt_table_like_parent2 (
    like mdt_table_local INCLUDING CONSTRAINTS
    ) DISTRIBUTED RANDOMLY;

--like parent_table

    CREATE TABLE mdt_table_like_parent3 (
    like mdt_table_local EXCLUDING DEFAULTS
    ) DISTRIBUTED RANDOMLY;

--like parent_table

    CREATE TABLE mdt_table_like_parent4 (
    like mdt_table_local EXCLUDING CONSTRAINTS
    ) DISTRIBUTED RANDOMLY;


drop table mdt_table_like_parent;
drop table mdt_table_like_parent1;
drop table mdt_table_like_parent2;
drop table mdt_table_like_parent3;
drop table mdt_table_like_parent4;
drop table mdt_table_local;

