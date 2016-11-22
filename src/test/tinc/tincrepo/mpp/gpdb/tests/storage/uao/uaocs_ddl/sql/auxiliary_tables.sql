-- @Description Checkes the names of auxiliary relations
-- 

CREATE TABLE t(a INT, b INT) WITH (appendonly=true, orientation=column) DISTRIBUTED BY (a);
CREATE INDEX tindex ON t(b);
SELECT position('pg_aocsseg_' in relname) FROM pg_class where oid IN (SELECT segrelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class WHERE relname='t'));
SELECT position('pg_aoblkdir_' in relname) FROM pg_class where oid IN (SELECT blkdirrelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class WHERE relname='t'));
SELECT position('pg_aoblkdir_' in relname) FROM pg_class where oid IN (SELECT blkdiridxid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class WHERE relname='t'));
SELECT position('pg_aovisimap_' in relname) FROM pg_class where oid IN (SELECT visimaprelid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class WHERE relname='t'));
SELECT position('pg_aovisimap_' in relname) FROM pg_class where oid IN (SELECT visimapidxid FROM pg_appendonly WHERE relid IN (SELECT oid FROM pg_class WHERE relname='t'));
