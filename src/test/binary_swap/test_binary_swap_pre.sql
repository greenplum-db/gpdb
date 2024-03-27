-- WARNING: This file is executed against the postgres database. If
-- objects are to be manipulated in other databases, make sure to
-- change to the correct database first.

\c regression;

-- Drop tables carrying rle with zstd compression. That is not available in
-- earlier minor versions.
DROP TABLE co_rle_zstd1;
DROP TABLE co_rle_zstd3;
DROP TABLE co_rle_zlib_to_zstd;
DROP TABLE co6;
