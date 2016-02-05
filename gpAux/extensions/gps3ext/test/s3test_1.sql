-- ========
-- PROTOCOL
-- ========

-- create the database functions
CREATE OR REPLACE FUNCTION write_to_s3() RETURNS integer AS
   '$libdir/gps3ext.so', 's3_export' LANGUAGE C STABLE;
CREATE OR REPLACE FUNCTION read_from_s3() RETURNS integer AS
    '$libdir/gps3ext.so', 's3_import' LANGUAGE C STABLE;

-- declare the protocol name along with in/out funcs
CREATE PROTOCOL s3ext (
    readfunc  = read_from_s3,
    writefunc = write_to_s3
);

-- Check out the catalog table
select * from pg_extprotocol;

drop table example;
create table example (id serial, name text) distributed by (id);
insert into example (name) values ('ssf');
insert into example (name) values ('abc');
insert into example (name) values ('deefe3');
insert into example (name) values ('e90jg');
insert into example (name) values ('br t34');
insert into example (name) values ('3r3r');
insert into example (name) values ('ccfee4');
insert into example (name) values ('hbafe');

-- create the external table with this protocol:
--   Use a url that the protocol knows how to parse later (you make it whatever you want)
CREATE WRITABLE EXTERNAL TABLE ext_w(like example)
    LOCATION('s3://demotextfile.txt')
FORMAT 'text'
DISTRIBUTED BY (id);

CREATE READABLE EXTERNAL TABLE ext_r(like example)
    LOCATION('s3://demotextfile.txt')
FORMAT 'text';

-- Use the external tables
INSERT into ext_w select * from example;

SELECT * FROM ext_r;

-- =======
-- CLEANUP
-- =======
DROP EXTERNAL TABLE ext_r;
DROP EXTERNAL TABLE ext_w;

DROP PROTOCOL s3ext;
