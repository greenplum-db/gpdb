/* gpcontrib/gp_toolkit/gp_toolkit--1.2--1.3.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION gp_toolkit UPDATE TO '1.3'" to load this file. \quit

-- This file exists just for providing an upgrade path from pre-1.3 to 1.3.
-- The real change between 1.2 and 1.3 is that we do not create gp_toolkit schema
-- within the extension and will make it available before creating the extension.
-- But that change won't be able to be done in an upgrade script, so pre-1.3
-- versions will continue to have gp_toolkit schema under gp_toolkit extension.
