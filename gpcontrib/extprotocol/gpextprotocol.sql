
-- ========
-- PROTOCOL
-- ========

-- create the database functions
CREATE OR REPLACE FUNCTION write_to_file() RETURNS integer AS
   '$libdir/gpextprotocol.so', 'demoprot_export' LANGUAGE C STABLE;
CREATE OR REPLACE FUNCTION read_from_file() RETURNS integer AS 
    '$libdir/gpextprotocol.so', 'demoprot_import' LANGUAGE C STABLE;

-- declare the protocol name along with in/out funcs
CREATE PROTOCOL demoprot (
    readfunc  = read_from_file, 
    writefunc = write_to_file
);


-- Check out the catalog table
select * from pg_extprotocol;


-- create the external table with this protocol:
--   Use a url that the protocol knows how to parse later (you make it whatever you want)
CREATE WRITABLE EXTERNAL TABLE ext_w(like example)
--            .........
    LOCATION('demoprot://demotextfile.txt') 
--            ^^^^^^^^^   
FORMAT 'text'
DISTRIBUTED BY (id);


CREATE READABLE EXTERNAL TABLE ext_r(like example)
--            .........
    LOCATION('demoprot://demotextfile.txt') 
--            ^^^^^^^^^
FORMAT 'text';


-- Use the external tables
INSERT into ext_w select * from example;

SELECT * FROM ext_r;

-- cat /gpdata/primary/gp*/demotextfile.txt


-- ==================
-- ACCESS PRIVILEDGES
-- ==================


-- An unprivledged user can't use the protocol:
set session authorization caleb;

CREATE EXTERNAL TABLE nopriv(like example)
    LOCATION('demoprot://demotextfile.txt') 
FORMAT 'text';

set session authorization cwelton;
GRANT ALL ON PROTOCOL demoprot to caleb;

select * from pg_extprotocol;

set session authorization caleb;

CREATE EXTERNAL TABLE withpriv(like example)
    LOCATION('demoprot://demotextfile.txt') 
FORMAT 'text';

select * from withpriv;

set session authorization cwelton;


-- =========
-- FORMATTER
-- =========

--
-- Define our binary formatter function
--
CREATE OR REPLACE FUNCTION formatter_export(record) RETURNS bytea 
AS '$libdir/gpformatter.so', 'formatter_export'
LANGUAGE C IMMUTABLE;

CREATE OR REPLACE FUNCTION formatter_import() RETURNS record
AS '$libdir/gpformatter.so', 'formatter_import'
LANGUAGE C IMMUTABLE;



CREATE WRITABLE EXTERNAL TABLE ext_w2(like example)
    LOCATION('demoprot://demobinfile.txt') 
-- ..............................................
   FORMAT 'CUSTOM' (FORMATTER 'formatter_export') 
-- ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
DISTRIBUTED BY (id);


CREATE EXTERNAL TABLE ext_r2(like example)
    LOCATION('demoprot://demobinfile.txt') 
-- ..............................................
   FORMAT 'CUSTOM' (FORMATTER 'formatter_import') 
-- ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
;

-- Use the external tables
INSERT into ext_w2 select * from example;

SELECT * FROM ext_r2;


-- head -1 /gpdata/primary/gp0/demobinfile.txt
-- ls -ltr /gpdata/primary/gp0/demo*file.txt


-- =======
-- CLEANUP
-- =======
DROP EXTERNAL TABLE ext_r;
DROP EXTERNAL TABLE ext_w;
DROP EXTERNAL TABLE ext_r2;
DROP EXTERNAL TABLE ext_w2;
DROP EXTERNAL TABLE withpriv;
DROP PROTOCOL demoprot;

-- rm /gpdata/primary/gp*/demo*file.txt
