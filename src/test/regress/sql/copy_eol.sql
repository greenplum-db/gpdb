--
-- copy data from file with different EOL and NEWLINE option.
--
CREATE TABLE copy_eol(id int, seq text);

--- EOL: '\n'
\!echo -n -e '1\x1es1\\\n2\x1es2\\\n' > /tmp/copy_lf.file;
\COPY copy_eol FROM '/tmp/copy_lf.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b', NEWLINE 'LF');
\COPY copy_eol FROM '/tmp/copy_lf.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b'); 

--- EOL: '\r'
\!echo -n -e '1\x1es1\\\r2\x1es2\\\r' > /tmp/copy_cr.file;
\COPY copy_eol FROM '/tmp/copy_cr.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b', NEWLINE 'CR');
\COPY copy_eol FROM '/tmp/copy_cr.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b');

--- EOL: '\r\n'
\!echo -n -e '1\x1es1\\\r\n2\x1es2\\\r\n' > /tmp/copy_crlf.file;
\COPY copy_eol FROM '/tmp/copy_crlf.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b', NEWLINE 'CRLF');
\COPY copy_eol FROM '/tmp/copy_crlf.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b');

SELECT count(*) FROM copy_eol;

DROP TABLE copy_eol;
