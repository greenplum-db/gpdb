SHOW server_version;

CREATE TABLE ao_users (id integer, name text) WITH (appendonly=true) DISTRIBUTED BY (id);
1:BEGIN;
1:INSERT INTO ao_users VALUES (1, 'Jane');
1:INSERT INTO ao_users VALUES (2, 'John');

2:BEGIN;
2:INSERT INTO ao_users VALUES (3, 'Joe');

1:END;
2:END;
