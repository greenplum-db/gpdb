SHOW server_version;

CREATE TABLE aocs_users (id integer, name text) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (id);
1:BEGIN;
1:INSERT INTO aocs_users VALUES (1, 'Jane');
1:INSERT INTO aocs_users VALUES (2, 'John');

2:BEGIN;
2:INSERT INTO aocs_users VALUES (3, 'Joe');

1:END;
2:END;
