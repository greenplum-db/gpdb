-- Additional tests that exercise the BRIN user interface
CREATE TABLE brin_interface(i int);
INSERT INTO brin_interface SELECT * FROM generate_series(1, 5000);
CREATE INDEX ON brin_interface USING brin(i) WITH (pages_per_range=1);

-- Helper view to see revmap page contents.
CREATE VIEW revmap_page1 AS (SELECT * FROM
    brin_revmap_data(get_raw_page('brin_interface_i_idx', 1)) WHERE pages != '(0,0)' ORDER BY 1);

-- Sanity: there are 2 ranges on each QE.
SELECT gp_execution_segment(), * FROM gp_dist_random('revmap_page1') ORDER BY 1;
SELECT gp_execution_segment(), (brin_page_items(get_raw_page('brin_interface_i_idx', 2),
    'brin_interface_i_idx')).* from gp_dist_random('gp_id') ORDER BY 1;

-- Now desummarize the 1st range on each QE.
SELECT brin_desummarize_range('brin_interface_i_idx', 0);

-- Sanity: the 1st range is desummarized on each QE.
SELECT gp_execution_segment(), * FROM gp_dist_random('revmap_page1') ORDER BY 1;
SELECT gp_execution_segment(), (brin_page_items(get_raw_page('brin_interface_i_idx', 2),
    'brin_interface_i_idx')).* from gp_dist_random('gp_id') ORDER BY 1;

-- Now re-summarize the 1st range on each QE.
SELECT brin_summarize_range('brin_interface_i_idx', 0);

-- Sanity: the 1st range is desummarized on each QE.
SELECT gp_execution_segment(), * FROM gp_dist_random('revmap_page1') ORDER BY 1;
SELECT gp_execution_segment(), (brin_page_items(get_raw_page('brin_interface_i_idx', 2),
     'brin_interface_i_idx')).* from gp_dist_random('gp_id') ORDER BY 1;

-- Insert some more rows
INSERT INTO brin_interface SELECT * FROM generate_series(5001, 10000);

-- Summarize new ranges on all QEs (2 for each QE)
SELECT brin_summarize_new_values('brin_interface_i_idx');

-- Sanity: all ranges are summarized on all QEs.
SELECT gp_execution_segment(), * FROM gp_dist_random('revmap_page1') ORDER BY 1;
SELECT gp_execution_segment(), (brin_page_items(get_raw_page('brin_interface_i_idx', 2),
    'brin_interface_i_idx')).* from gp_dist_random('gp_id') ORDER BY 1;

-- Error out when autosummarize is specified as true during index build.
CREATE INDEX brin_interface_error_idx ON brin_interface USING brin(i) WITH (autosummarize=true);
-- Don't error out when autosummarize is specified as false during index build.
CREATE INDEX brin_interface_idx ON brin_interface USING brin(i) WITH (autosummarize=false);
-- Altering it to false should not result in an error.
ALTER INDEX brin_interface_idx SET (autosummarize=false);
-- Altering it to true should result in an error.
ALTER INDEX brin_interface_idx SET (autosummarize=true);

-- Test BRIN pages_per_range defaults
CREATE TABLE brin_ppr_heap1(i int);
CREATE INDEX ON brin_ppr_heap1 USING brin(i);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_heap1_i_idx', 0));

CREATE UNLOGGED TABLE brin_ppr_heap2(i int);
CREATE INDEX ON brin_ppr_heap2 USING brin(i);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_heap2_i_idx', 0));

CREATE TABLE brin_ppr_heap3(i int);
CREATE INDEX ON brin_ppr_heap3 USING brin(i) WITH (autosummarize=false);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_heap3_i_idx', 0));

CREATE TABLE brin_ppr_ao_row1(i int) USING ao_row;
CREATE INDEX ON brin_ppr_ao_row1 USING brin(i);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_row1_i_idx', 0));

CREATE UNLOGGED TABLE brin_ppr_ao_row2(i int) USING ao_row;
CREATE INDEX ON brin_ppr_ao_row2 USING brin(i);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_row2_i_idx', 0));

CREATE TABLE brin_ppr_ao_row3(i int) USING ao_row;
CREATE INDEX ON brin_ppr_ao_row3 USING brin(i) WITH (autosummarize=false);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_row3_i_idx', 0));

CREATE TABLE brin_ppr_ao_column1(i int) USING ao_column;
CREATE INDEX ON brin_ppr_ao_column1 USING brin(i);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_column1_i_idx', 0));

CREATE UNLOGGED TABLE brin_ppr_ao_column2(i int) USING ao_column;
CREATE INDEX ON brin_ppr_ao_column2 USING brin(i);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_column2_i_idx', 0));

CREATE TABLE brin_ppr_ao_column3(i int) USING ao_column;
CREATE INDEX ON brin_ppr_ao_column3 USING brin(i) WITH (autosummarize=false);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_column3_i_idx', 0));

-- Test ALTERing pages_per_range
-- Use REINDEX too as pages_per_range change won't go into effect until next reindex
ALTER INDEX brin_ppr_heap1_i_idx SET (pages_per_range=16);
REINDEX INDEX brin_ppr_heap1_i_idx;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_heap1_i_idx', 0));
ALTER INDEX brin_ppr_heap1_i_idx RESET (pages_per_range);
REINDEX INDEX brin_ppr_heap1_i_idx;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_heap1_i_idx', 0));

ALTER INDEX brin_ppr_ao_row1_i_idx SET (pages_per_range=16);
REINDEX INDEX brin_ppr_ao_row1_i_idx;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_row1_i_idx', 0));
ALTER INDEX brin_ppr_ao_row1_i_idx RESET (pages_per_range);
REINDEX INDEX brin_ppr_ao_row1_i_idx;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_row1_i_idx', 0));

ALTER INDEX brin_ppr_ao_column1_i_idx SET (pages_per_range=16);
REINDEX INDEX brin_ppr_ao_column1_i_idx;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_column1_i_idx', 0));
ALTER INDEX brin_ppr_ao_column1_i_idx RESET (pages_per_range);
REINDEX INDEX brin_ppr_ao_column1_i_idx;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_ao_column1_i_idx', 0));

-- ALTERIng the ACCESS METHOD of the table should gracefully set the default
-- pages_per_range for the associated BRIN index.
CREATE TABLE brin_ppr_atsetam(i int) USING heap;
CREATE INDEX ON brin_ppr_atsetam USING brin(i);
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_atsetam_i_idx', 0));

ALTER TABLE brin_ppr_atsetam SET ACCESS METHOD ao_row;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_atsetam_i_idx', 0));

ALTER TABLE brin_ppr_atsetam SET ACCESS METHOD heap;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_atsetam_i_idx', 0));

-- ALTERing the ACCESS METHOD of the table should NOT set the default
-- pages_per_range if the index had a pages_per_range set.
ALTER INDEX brin_ppr_atsetam_i_idx SET (pages_per_range=8);
REINDEX INDEX brin_ppr_atsetam_i_idx;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_atsetam_i_idx', 0));
ALTER TABLE brin_ppr_atsetam SET ACCESS METHOD ao_row;
SELECT pagesperrange FROM brin_metapage_info(get_raw_page('brin_ppr_atsetam_i_idx', 0));
