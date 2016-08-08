-- @product_version gpdb: [4.3.6.2 - 4.3.99.0)
DROP TABLE IF EXISTS rle_block_boundary;
CREATE TABLE rle_block_boundary (a int, b text) with (appendonly=true, orientation=column, compresstype=rle_type);
COPY rle_block_boundary(b) FROM '%PATH%/data/rle_block_boundary';
SELECT COUNT(*) FROM rle_block_boundary;
