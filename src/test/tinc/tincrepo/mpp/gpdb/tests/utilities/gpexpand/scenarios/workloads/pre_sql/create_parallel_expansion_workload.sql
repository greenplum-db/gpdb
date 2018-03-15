CREATE TABLESPACE test_ts;

DROP TABLE IF EXISTS users_dur_pkey;
CREATE TABLE users_dur_pkey (
    id    SERIAL PRIMARY KEY,
    name  TEXT,
    age   INT4
);
ALTER TABLE users_dur_pkey set TABLESPACE test_ts;
INSERT INTO users_dur_pkey select i, 'name'||i, i%50+1 from generate_series(1,100000) i;

DROP TABLE IF EXISTS users_duration_4;
CREATE TABLE users_duration_4(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a);
INSERT INTO users_duration_4  
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 100000)i;

DROP TABLE IF EXISTS users_duration_5;
CREATE TABLE users_duration_5(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a);
INSERT INTO users_duration_5  
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 100000)i;

DROP TABLE IF EXISTS users_duration_6;
CREATE TABLE users_duration_6(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a);
INSERT INTO users_duration_6  
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 100000)i;

