set enable_seqscan=off;

CREATE TABLE test_char (
	i "char"
);

INSERT INTO test_char VALUES ('a'),('b'),('c'),('d'),('e'),('f');

CREATE INDEX idx_char ON test_char USING gin (i);

SELECT * FROM test_char WHERE i<'d'::"char" ORDER BY i;
SELECT * FROM test_char WHERE i<='d'::"char" ORDER BY i;
SELECT * FROM test_char WHERE i='d'::"char" ORDER BY i;
SELECT * FROM test_char WHERE i>='d'::"char" ORDER BY i;
SELECT * FROM test_char WHERE i>'d'::"char" ORDER BY i;
