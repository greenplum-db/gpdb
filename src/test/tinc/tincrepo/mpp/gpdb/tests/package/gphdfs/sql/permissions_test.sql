/* Note that roles are defined at the system-level and are valid
 * for all databases in your Greenplum Database system. */
\echo '-- start_ignore'
revoke all on protocol gphdfs from _hadoop_perm_test_role;
DROP ROLE IF EXISTS _hadoop_perm_test_role;
\echo '-- end_ignore'

/* Now create a new role. Initially this role should NOT
 * be allowed to create an external hdfs table. */

CREATE ROLE _hadoop_perm_test_role
WITH CREATEEXTTABLE
LOGIN;

\set OLD_GP_USER :USER
\connect - _hadoop_perm_test_role;


/* The following MUST fail! */
CREATE EXTERNAL TABLE dummy_greek (
	letter CHARACTER(1),
	english_word VARCHAR,
	some_array DOUBLE PRECISION[]
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/greek_utf8.txt')
FORMAT 'TEXT'
ENCODING 'UTF8';

/* Writing data must also fail */
CREATE WRITABLE EXTERNAL TABLE export_dummy_greek (
	letter CHARACTER(1),
	english_word VARCHAR,
	some_array DOUBLE PRECISION[]
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/export')
FORMAT 'TEXT' (
    DELIMITER ';'
)
ENCODING 'UTF8';


\connect - :OLD_GP_USER

--ALTER ROLE _hadoop_perm_test_role
--WITH
--CREATEEXTTABLE(type='writable', protocol='gphdfs');
grant insert on protocol gphdfs to _hadoop_perm_test_role;

\connect - _hadoop_perm_test_role;


/* The following MUST still fail! */
CREATE EXTERNAL TABLE dummy_greek (
	letter CHARACTER(1),
	english_word VARCHAR,
	some_array DOUBLE PRECISION[]
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/greek_utf8.txt')
FORMAT 'TEXT'
ENCODING 'UTF8';

/* On the other hand, creating the writable external table must work now. */
CREATE WRITABLE EXTERNAL TABLE export_dummy_greek (
	letter CHARACTER(1),
	english_word VARCHAR,
	some_array DOUBLE PRECISION[]
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/export')
FORMAT 'TEXT' (
    DELIMITER ';'
)
ENCODING 'UTF8';

INSERT INTO export_dummy_greek
SELECT * FROM greek;


\connect - :OLD_GP_USER

--ALTER ROLE _hadoop_perm_test_role
--WITH
--CREATEEXTTABLE(type='readable', protocol='gphdfs')
--CREATEEXTTABLE(type='writable');
grant all on protocol gphdfs to _hadoop_perm_test_role;

\connect - _hadoop_perm_test_role


/* The following should finally work! */
CREATE EXTERNAL TABLE dummy_greek (
	letter CHARACTER(1),
	english_word VARCHAR,
	some_array DOUBLE PRECISION[]
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/greek_utf8.txt')
FORMAT 'TEXT'
ENCODING 'UTF8';

SELECT some_array[2] FROM dummy_greek
ORDER BY letter
OFFSET 1 LIMIT 1;

DROP EXTERNAL TABLE dummy_greek;
DROP EXTERNAL TABLE export_dummy_greek;

\connect - :OLD_GP_USER
revoke all on protocol gphdfs from _hadoop_perm_test_role;
DROP ROLE IF EXISTS _hadoop_perm_test_role;
