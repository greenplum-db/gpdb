CREATE TABLE abort_create_needed_cr_co_reindex_table_bitmap_index(
text_col text,
bigint_col bigint,
char_vary_col character varying(30),
numeric_col numeric,
int_col int4,
float_col float4,
int_array_col int[],
drop_col numeric,
before_rename_col int4,
change_datatype_col numeric,
a_ts_without timestamp without time zone,
b_ts_with timestamp with time zone,
date_column date) WITH (appendonly=true,orientation=column);

CREATE INDEX abort_create_needed_cr_co_reindex_bitmap_idx1 ON abort_create_needed_cr_co_reindex_table_bitmap_index using bitmap(numeric_col);

insert into abort_create_needed_cr_co_reindex_table_bitmap_index values ('0_zero', 0, '0_zero', 0, 0, 0, '{0}', 0, 0, 0, '2004-10-19 10:23:54', '2004-10-19 10:23:54+02', '1-1-2000');
insert into abort_create_needed_cr_co_reindex_table_bitmap_index values ('1_zero', 1, '1_zero', 1, 1, 1, '{1}', 1, 1, 1, '2005-10-19 10:23:54', '2005-10-19 10:23:54+02', '1-1-2001');
insert into abort_create_needed_cr_co_reindex_table_bitmap_index values ('2_zero', 2, '2_zero', 2, 2, 2, '{2}', 2, 2, 2, '2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002');
insert into abort_create_needed_cr_co_reindex_table_bitmap_index select i||'_'||repeat('text',100),i,i||'_'||repeat('text',3),i,i,i,'{3}',i,i,i,'2006-10-19 10:23:54', '2006-10-19 10:23:54+02', '1-1-2002' from generate_series(3,100)i;

SELECT COUNT(*) FROM abort_create_needed_cr_co_reindex_table_bitmap_index;
