-- @Description Tests dropping a column after a compaction with update
CREATE TABLE uaocs_drop_column_update(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
int_array_col int[],
drop_col numeric) with (appendonly=true, orientation=column);

INSERT INTO uaocs_drop_column_update values ('1_zero', 1, '1_zero', '{1}', 1);
ALTER TABLE uaocs_drop_column_update DROP COLUMN drop_col;
Select char_vary_col, int_array_col from uaocs_drop_column_update;
INSERT INTO uaocs_drop_column_update values ('2_zero', 2, '2_zero', '{2}');
update uaocs_drop_column_update set bigint_col = bigint_col + 1 where text_col = '1_zero';
Select char_vary_col, int_array_col from uaocs_drop_column_update;
