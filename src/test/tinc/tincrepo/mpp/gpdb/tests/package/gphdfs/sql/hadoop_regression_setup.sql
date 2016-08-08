\echo '-- start_ignore'
DROP EXTERNAL TABLE IF EXISTS houses;
DROP EXTERNAL TABLE IF EXISTS greek;
DROP EXTERNAL TABLE IF EXISTS four_numbers;
DROP EXTERNAL TABLE IF EXISTS four_numbers_no_LF_before_EOF;
DROP EXTERNAL TABLE IF EXISTS export_greek;
DROP EXTERNAL TABLE IF EXISTS export_houses;
\echo '-- end_ignore'



/* Readable external tables */

CREATE READABLE EXTERNAL TABLE houses (
	id INT,
	tax INT,
	bedroom INT,
	bath FLOAT,
	price INT,
	size INT,
	lot INT
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/houses.txt')
FORMAT 'TEXT' (
	DELIMITER '|'
);
GRANT ALL ON TABLE houses TO PUBLIC;


CREATE READABLE EXTERNAL TABLE greek (
	letter CHARACTER(1),
	english_word VARCHAR,
	some_array DOUBLE PRECISION[]
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/greek_utf8.txt')
FORMAT 'TEXT'
ENCODING 'UTF8';
GRANT ALL ON TABLE greek TO PUBLIC;


CREATE READABLE EXTERNAL TABLE four_numbers (
	num INT
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/four_numbers.txt')
FORMAT 'TEXT';
GRANT ALL ON TABLE four_numbers TO PUBLIC;

CREATE READABLE EXTERNAL TABLE four_numbers_no_LF_before_EOF (
	num INT
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/four_numbers_no_LF_before_EOF.txt')
FORMAT 'TEXT';
GRANT ALL ON TABLE four_numbers TO PUBLIC;

/* Writable external tables */
/* Note the / at the end of the Location URI. This should not fail. */

CREATE WRITABLE EXTERNAL TABLE export_greek (
	letter CHARACTER(1),
	english_word VARCHAR,
	some_array DOUBLE PRECISION[]
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/export/')
FORMAT 'TEXT' (
	DELIMITER E'\t'
);

CREATE WRITABLE EXTERNAL TABLE export_houses (
	id INT,
	tax INT,
	bedroom INT,
	bath FLOAT,
	price INT,
	size INT,
	lot INT
)
LOCATION ('gphdfs://%HADOOP_HOST%/plaintext/export')
FORMAT 'TEXT' (
	DELIMITER E'\t'
);

