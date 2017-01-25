-- @gucs gp_create_table_random_default_distribution=off
drop schema if exists globaltest_schema;
drop database if exists dumpglobaltest;
drop role if exists globaltest_role;

-- Create test database dumpglobaltest
create database dumpglobaltest;
\c dumpglobaltest

-- Create a role
create role globaltest_role login;

-- Grant privillige
grant create on database dumpglobaltest to globaltest_role;

-- Create a custom schema
create schema globaltest_schema authorization globaltest_role;

