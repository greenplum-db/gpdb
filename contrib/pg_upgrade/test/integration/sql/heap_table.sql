SHOW server_version;

CREATE TABLE users (id INTEGER, name TEXT) DISTRIBUTED BY (id);
INSERT INTO users VALUES (1, 'Jane');
INSERT INTO users VALUES (2, 'John');
INSERT INTO users VALUES (3, 'Joe');
