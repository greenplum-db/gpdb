-- Some additional checks for RULEs

-- Test turning a table into a view.

CREATE table table_to_view_test1 (a int);
CREATE table table_to_view_test2 (a int);
CREATE rule "_RETURN" as on select to table_to_view_test1
        do instead select * from table_to_view_test2;

-- relkind has been changed to 'v'
SELECT relkind FROM pg_class
    WHERE oid = 'table_to_view_test1'::regclass;
-- distribution policy record has been deleted
SELECT 1 FROM gp_distribution_policy
    WHERE localoid = 'table_to_view_test1'::regclass;

DROP VIEW table_to_view_test1;
DROP TABLE table_to_view_test2;

-- Same for an Append-Only table. It is currently not supported.

CREATE table aotable_to_view_test1 (a int) with (appendonly=true);
CREATE table aotable_to_view_test2 (a int);
CREATE rule "_RETURN" as on select to aotable_to_view_test1
        do instead select * from aotable_to_view_test2;

drop table aotable_to_view_test1;
drop table aotable_to_view_test2;
