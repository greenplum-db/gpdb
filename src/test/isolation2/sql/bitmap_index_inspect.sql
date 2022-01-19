-- This tests the bitmap index pageinspect functions. The tests reside here, as opposed to
-- contrib/pageinspect because we want to leverage isolation2's utility mode syntax (since the
-- inspect functions run against a single node, as opposed to the entire GP cluster)

-- Setup
1U: CREATE EXTENSION pageinspect;
1U: CREATE TABLE bmtest_t1(i int, bmfield int);
1U: CREATE INDEX bmtest_i1 ON bmtest_t1 USING bitmap(bmfield);
1U: INSERT INTO bmtest_t1 SELECT i,1 FROM generate_series(1, 1000) i;
1U: INSERT INTO bmtest_t1 SELECT i,2 FROM generate_series(1, 1000) i;

-- start_matchsubs
-- m/bmfuncs.c:\d+/
-- s/bmfuncs.c:\d+/bmfuncs.c:XXX/g
-- end_matchsubs

-- Test metapage
1U: SELECT magic, version, regexp_replace(auxrelid::regclass::text,'[[:digit:]]+', 'auxrelid') AS auxrelname,
           regexp_replace(auxindexrelid::regclass::text,'[[:digit:]]+', 'auxindrelid') AS auxindexrelname FROM bm_metap('bmtest_i1');

-- Test LOV item pages
-- Negative cases (not a LOV page)
1U: SELECT * FROM bm_lov_page_items('bmtest_i1', 0);
1U: SELECT * FROM bm_lov_page_items('bmtest_i1', 2);
-- Positive test
1U: SELECT * FROM bm_lov_page_items('bmtest_i1', 1) order by itemoffset;

-- Test bitmap pages
-- Negative cases (not a bitmap page)
1U: SELECT * FROM bm_bitmap_page_items('bmtest_i1', 0);
1U: SELECT * FROM bm_bitmap_page_items('bmtest_i1', 1);
-- Positive test
1U: SELECT * FROM bm_bitmap_page_header('bmtest_i1', 2);
1U: SELECT * FROM bm_bitmap_page_items('bmtest_i1', 2) order by word_num;

-- cleanup
1U: DROP TABLE bmtest_t1;
1U: DROP EXTENSION pageinspect;
