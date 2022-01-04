/* contrib/pageinspect/pageinspect--1.7--1.8.sql */

-- complain if script is sourced in psql, rather than via ALTER EXTENSION
\echo Use "ALTER EXTENSION pageinspect UPDATE TO '1.8'" to load this file. \quit

--
-- bm_metap()
--
CREATE FUNCTION bm_metap(IN relname text,
     OUT magic int4,
     OUT version int4,
     OUT auxrelid oid,
     OUT auxindexrelid oid,
     OUT lovlastblknum bigint)
AS 'MODULE_PATHNAME', 'bm_metap'
LANGUAGE C STRICT PARALLEL SAFE;

--
-- bm_lov_page_items()
--
CREATE FUNCTION bm_lov_page_items(IN relname text,
                                  IN blkno int4,
                                  OUT itemoffset smallint,
                                  OUT lov_head_blkno bigint,
                                  OUT lov_tail_blkno bigint,
                                  OUT last_complete_word text,
                                  OUT last_word text,
                                  OUT last_tid numeric,
                                  OUT last_setbit_tid numeric,
                                  OUT is_last_complete_word_fill bool,
                                  OUT is_last_word_fill bool)
    RETURNS SETOF record
AS 'MODULE_PATHNAME', 'bm_lov_page_items'
    LANGUAGE C STRICT PARALLEL SAFE;

--
-- bm_bitmap_page_header()
--
CREATE FUNCTION bm_bitmap_page_header(IN relname text,
                         IN blkno int4,
                         OUT num_words bigint,
                         OUT next_blkno bigint,
                         OUT last_tid numeric)
AS 'MODULE_PATHNAME', 'bm_bitmap_page_header'
    LANGUAGE C STRICT PARALLEL SAFE;

--
-- bm_bitmap_page_items()
--
CREATE FUNCTION bm_bitmap_page_items(IN relname text,
                                  IN blkno int4,
                                  OUT word_num bigint,
                                  OUT compressed bool,
                                  OUT content_word text)
    RETURNS SETOF record
AS 'MODULE_PATHNAME', 'bm_bitmap_page_items'
    LANGUAGE C STRICT PARALLEL SAFE;

--
-- bm_bitmap_page_items_bytea()
--
CREATE FUNCTION bm_bitmap_page_items(IN page bytea,
                                  OUT word_num bigint,
                                  OUT compressed bool,
                                  OUT content_word text)
    RETURNS SETOF record
AS 'MODULE_PATHNAME', 'bm_bitmap_page_items_bytea'
    LANGUAGE C STRICT PARALLEL SAFE;
