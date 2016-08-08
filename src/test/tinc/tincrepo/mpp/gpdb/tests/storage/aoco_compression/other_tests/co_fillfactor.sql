-- start_ignore
SET gp_create_table_random_default_distribution=off;
-- end_ignore
-- MPP-6714:
-- FILLFACTOR should not apply to append-only tables, and thus should not apply 
-- to Column-oriented tables.  Yet apparently we accept it.

-- start_ignore
DROP TABLE IF EXISTS cos_table81a;
DROP TABLE IF EXISTS cos_table81b;
DROP TABLE IF EXISTS cos_table81c;
-- end_ignore


-- The following should be invalid.
CREATE TABLE cos_table81a (id BIGSERIAL, tincan FLOAT)
 WITH (
    AppendOnly=True,
    Orientation='column',
    FillFactor = 10
    )
 DISTRIBUTED BY (id)
 ;


-- The bug is not specific to column-oriented tables; plain old AO tables 
-- also incorrectly allow you to specify appendonly and fillfactor
CREATE TABLE cos_table81b (id BIGSERIAL, tincan FLOAT)
 WITH (
    AppendOnly=True,
    FillFactor = 10
    )
 DISTRIBUTED BY (id)
 ;


-- Clean up.
-- start_ignore
DROP TABLE IF EXISTS cos_table81a;
DROP TABLE IF EXISTS cos_table81b;
-- end_ignore

