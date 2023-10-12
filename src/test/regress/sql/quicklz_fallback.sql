-- quicklz compression is not supported in GPDB7.
-- If the GUC gp_quicklz_fallback = false, setting compresstype=quicklz via CREATE/ALTER/gp_default_storage_options will result in an ERROR.
-- if ZSTD is available, setting gp_quicklz_fallback = true will cause the server to fall back to using ZSTD.
-- If ZSTD is not available, it will instead fall back to using AO_DEFAULT_USABLE_COMPRESSTYPE.
-- This file tests that gp_quicklz_fallback=true will cause the server to correctly fall back to a valid usable compresstype.

-- The tests shouldn't care which compresstype we fall back to, since it depends on how the server is configured.
-- Just ensure that it's a valid usable compresstype  (i.e. the commands don't error out unexpectedly)

-- start_matchsubs
-- m/zstd/
-- s/zstd/VALID/g
-- m/zlib/
-- s/zlib/VALID/g
-- m/none
-- s/none/VALID/g
-- end_matchsubs

-- Ensure statements correctly ERROR when gp_quicklz_fallback=false.
SET gp_quicklz_fallback = false;

CREATE TABLE quicklz_err(c1 int) USING ao_column WITH (compresstype=quicklz) DISTRIBUTED BY (c1);

CREATE TABLE quicklz_err(c1 int ENCODING (compresstype=quicklz)) USING ao_column DISTRIBUTED BY (c1);
CREATE TABLE quicklz_err(c1 int) USING ao_column DISTRIBUTED BY (c1);
ALTER TABLE quicklz_err ADD COLUMN c2 int ENCODING (compresstype=quicklz);
ALTER TABLE quicklz_err ALTER COLUMN c1 SET ENCODING (compresstype=quicklz);

SET gp_default_storage_options='compresstype=quicklz';
SHOW gp_default_storage_options;

DROP TABLE quicklz_err;

-- Ensure statements correctly fall back to a different compresstype when gp_quicklz_fallback=true.
SET gp_quicklz_fallback = true;
SET gp_default_storage_options='';

-- with gp_quicklz_fallback set to true, create table using other compress type
-- should not be impacted
CREATE TABLE zlib_with(a int) USING ao_column WITH (compresstype=zlib) DISTRIBUTED BY (a);
select reloptions::text like '%compresstype=zlib%' as ok from pg_class where oid = 'zlib_with'::regclass::oid;
DROP TABLE zlib_with;

-- Fill in column encoding from WITH clause
CREATE TABLE quicklz_with(c1 int, c2 int) USING ao_column WITH (compresstype=quicklz) DISTRIBUTED BY (c1);
\d+ quicklz_with

-- CREATE/ALTER TABLE ADD using column ENCODING
CREATE TABLE quicklz_colencoding(c1 int ENCODING (compresstype=quicklz)) USING ao_column DISTRIBUTED BY (c1);
ALTER TABLE quicklz_colencoding ADD COLUMN c2 int ENCODING (compresstype=quicklz);
ALTER TABLE quicklz_colencoding ADD COLUMN c3 int;
\d+ quicklz_colencoding

-- ALTER COLUMN
ALTER TABLE quicklz_colencoding ALTER COLUMN c3 SET ENCODING (compresstype=quicklz, compresslevel=1);
\d+ quicklz_colencoding


-- Partitioned table
CREATE TABLE quicklz_pt(c1 int, c2 int) USING ao_column WITH (compresstype=quicklz) DISTRIBUTED BY (c1) PARTITION BY RANGE(c2) (START (1) END (3));
ALTER TABLE quicklz_pt ADD COLUMN c3 int ENCODING (compresstype=quicklz);
ALTER TABLE quicklz_pt ALTER COLUMN c3 SET ENCODING (compresstype=quicklz, compresslevel=1);
\d+ quicklz_pt
\d+ quicklz_pt_1_prt_1


SET gp_default_storage_options='compresstype=quicklz';
SHOW gp_default_storage_options;

CREATE TABLE quicklz_default_opts(c1 int, c2 int) USING ao_column DISTRIBUTED BY (c1);
\d+ quicklz_default_opts
