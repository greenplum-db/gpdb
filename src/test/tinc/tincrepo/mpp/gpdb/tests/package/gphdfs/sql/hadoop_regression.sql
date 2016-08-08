/* Suppress notice messages */

SET client_min_messages = error;

/* Basic testing: Super user read. */

SELECT * FROM houses ORDER BY id;

SELECT * FROM greek ORDER BY letter;

SELECT count(*) FROM four_numbers;

SELECT count(*) FROM four_numbers_no_LF_before_EOF;

/* Basic testing: Super user write */

\!%HADOOP_FS%/bin/hadoop fs -rmr '/plaintext/export/*' &> /dev/null

INSERT INTO export_houses
SELECT * FROM houses;

\!%HADOOP_FS%/bin/hadoop fs -cat '/plaintext/export/*' 2> /dev/null | sort --general-numeric-sort
