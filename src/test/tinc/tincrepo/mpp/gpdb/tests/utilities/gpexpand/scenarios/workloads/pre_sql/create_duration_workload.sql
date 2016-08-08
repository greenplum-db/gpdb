DROP TABLE IF EXISTS users_dur_pkey;
CREATE TABLE users_dur_pkey (
    id    SERIAL PRIMARY KEY,
    name  TEXT,
    age   INT4
);
INSERT INTO users_dur_pkey select i, 'name'||i, i%50+1 from generate_series(1,1000) i;

DROP TABLE IF EXISTS users_duration_2;
CREATE TABLE users_duration_2(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_2
SELECT i, (i * 10) % 100, i::text, '2012-09-01 03:04:05'
FROM generate_series(1, 5000)i;

DROP TABLE IF EXISTS users_duration_3;
CREATE TABLE users_duration_3(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_3
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 5000)i;


DROP TABLE IF EXISTS users_duration_4;
CREATE TABLE users_duration_4(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_4  
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000000)i;

DROP TABLE IF EXISTS users_duration_5;
CREATE TABLE users_duration_5(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_5  
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000000)i;

DROP TABLE IF EXISTS users_duration_6;
CREATE TABLE users_duration_6(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_6  
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000000)i;

DROP TABLE IF EXISTS users_duration_7;
CREATE TABLE users_duration_7(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_7
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000000)i;


DROP TABLE IF EXISTS users_duration_8;
CREATE TABLE users_duration_8(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_8
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000000)i;

DROP TABLE IF EXISTS users_duration_9;
CREATE TABLE users_duration_9(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_9
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000000)i;



DROP TABLE IF EXISTS users_duration_10;
CREATE TABLE users_duration_10(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_10
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000000)i;

DROP TABLE IF EXISTS users_duration_11;
CREATE TABLE users_duration_11(
    a text,
    b int,
    c text,
    d timestamp
)
distributed by (a)
PARTITION BY RANGE(b) (START (0) END (100) INCLUSIVE EVERY(20));
INSERT INTO users_duration_11
SELECT i, (i * 10) % 100, i::text, '2013-09-01 03:04:05'
FROM generate_series(1, 1000000)i;


