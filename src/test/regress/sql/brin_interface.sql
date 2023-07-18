-- Additional tests that exercise the BRIN user interface
CREATE EXTENSION pageinspect;

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

DROP EXTENSION pageinspect CASCADE;

