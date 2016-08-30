-- Misc tests related to dispatching queries to segments.

-- Test quoting of GUC values and databse names when they're sent to segments

-- There used to be a bug in the quoting when the search_path setting was sent
-- to the segment. It was not easily visible when search_path was set with a
-- SET command, only when the setting was sent as part of the startup packet.
-- Set search_path as a per-user setting so that we can test that.
CREATE DATABASE "dispatch test db";
ALTER DATABASE "dispatch test db" SET search_path="my schema",public;

\c "dispatch test db"

CREATE SCHEMA "my schema";

-- Create a table with the same name in both schemas, "my schema" and public.
CREATE TABLE "my table" (t text);
INSERT INTO "my table" VALUES ('myschema.mytable');

CREATE TABLE public."my table" (t text);
INSERT INTO public."my table" VALUES ('public.mytable');

SELECT t as unquoted FROM "my table";
SELECT t as myschema FROM "my schema"."my table";
SELECT t as public FROM public."my table";

DROP TABLE "my table";
DROP TABLE public."my table";

-- Create another table with the same name. To make sure the DROP worked
-- and dropped the correct table.
CREATE TABLE "my table" (id integer);
DROP TABLE "my table";

-- Clean up
\c regression
DROP DATABASE "dispatch test db";

--
-- test QD will report failure if QE fails to send its motion_listener back
-- during backend initialization
--

-- start_ignore
\! gpfaultinjector -f send_qe_details_init_backend -y reset -s 2
-- inject a 'skip' fault before QE sends its motion_listener
\! gpfaultinjector -f send_qe_details_init_backend -y skip -s 2 -o 0
-- end_ignore

-- terminate exiting QEs first
\c
-- verify failure will be reported
SELECT 1 FROM gp_dist_random('gp_id');

-- reset fault injector
-- start_ignore
\! gpfaultinjector -f send_qe_details_init_backend -y reset -s 2
-- end_ignore
