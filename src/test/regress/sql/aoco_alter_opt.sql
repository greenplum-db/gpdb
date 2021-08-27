DROP TABLE IF EXISTS aoco_opt;
CREATE TABLE aoco_opt (
    a integer,
    b integer default 3 NOT NULL encoding(compresstype=zlib,compresslevel=2),
    c integer,
    default column encoding (compresstype=RLE_TYPE)
) WITH (
    appendonly=true,
    orientation=column
) DISTRIBUTED BY (a);

PREPARE attopts AS SELECT
    attname,
    atttypid,
    pga.attnum,
    attisdropped,
    attislocal,
    attstorage,
    attinhcount,
    pge.attoptions
FROM
    pg_attribute_encoding pge,
    pg_attribute pga,
    pg_class pgc
WHERE
    pga.attrelid = pgc.oid AND
    pga.attnum = pge.attnum AND
    pge.attrelid = pgc.oid AND
    pgc.relname = 'aoco_opt'; 

SELECT relfilenode AS origfilenode FROM pg_class WHERE relname = 'aoco_opt' \gset

EXECUTE attopts;

INSERT INTO aoco_opt SELECT a, a AS b, a AS c FROM generate_series(0, 9) a;

BEGIN;
ALTER TABLE aoco_opt ALTER COLUMN b SET DATA TYPE text;
EXECUTE attopts;
SELECT relname FROM pg_class WHERE relfilenode = :origfilenode;
TABLE aoco_opt;

-- This will fail due to silently dropped default
INSERT INTO aoco_opt (a, c) VALUES (10, 10);
ROLLBACK;

EXECUTE attopts;
TABLE aoco_opt;

