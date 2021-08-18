--
-- copy data to and from file with EOL.
--
CREATE TABLE copy_eol(id int, seq text);

INSERT INTO copy_eol select '1', 's1\';
INSERT INTO copy_eol select '2', 's2\';

-- copy data to a file then copy from it.
\COPY copy_eol TO '/tmp/copy_eol.file' (FORMAT 'text', DELIMITER E'\x1e', NULL E'\x1b\x4e',  ESCAPE E'\x1b');
\COPY copy_eol FROM '/tmp/copy_eol.file' (FORMAT 'text', DELIMITER E'\x1e' , NULL E'\x1b\x4e',  ESCAPE E'\x1b', NEWLINE 'LF');

SELECT count(*) FROM copy_eol;

DROP TABLE copy_eol;
