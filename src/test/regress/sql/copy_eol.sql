--
-- copy data from file with different EOL and NEWLINE option.
-- Using /usr/bin/printf in case of different behaviours on
-- different OS, especially on ubuntu18.
--
CREATE TABLE copy_eol(id int, seq text);

--- EOL: '\n'
\!/usr/bin/printf '1\036s1\134\n2\036s2\134\n' > /tmp/copy_lf.file;
COPY copy_eol FROM '/tmp/copy_lf.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b', NEWLINE 'LF');
COPY copy_eol FROM '/tmp/copy_lf.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b'); 
SELECT * FROM copy_eol ORDER BY id ASC;
TRUNCATE copy_eol;

--- EOL: '\r'
--- produce '\r' step by step on linux
\!/usr/bin/printf '1\036s1\' > /tmp/copy_cr.file;
\!/usr/bin/printf '\r' >> /tmp/copy_cr.file;
\!/usr/bin/printf '2\036s2\' >> /tmp/copy_cr.file;
\!/usr/bin/printf '\r' >> /tmp/copy_cr.file;
COPY copy_eol FROM '/tmp/copy_cr.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b', NEWLINE 'CR');
COPY copy_eol FROM '/tmp/copy_cr.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b');
SELECT * FROM copy_eol ORDER BY id ASC;
TRUNCATE copy_eol;

--- EOL: '\r\n'
\!/usr/bin/printf '1\036s1\\\r\n2\036s2\\\r\n' > /tmp/copy_crlf.file;
COPY copy_eol FROM '/tmp/copy_crlf.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b', NEWLINE 'CRLF');
COPY copy_eol FROM '/tmp/copy_crlf.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b');
SELECT * FROM copy_eol ORDER BY id ASC;
TRUNCATE copy_eol;

--- EOL: '\r\n' with a '\r' in data
\!/usr/bin/printf '1\036s1\\\r\n2\036s2\\\r\r\n3\036s3\\\r\n' > /tmp/copy_crlf_1.file;
COPY copy_eol FROM '/tmp/copy_crlf_1.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b', NEWLINE 'CRLF');
COPY copy_eol FROM '/tmp/copy_crlf_1.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b');
SELECT * FROM copy_eol ORDER BY id ASC;
TRUNCATE copy_eol;

DROP TABLE copy_eol;
