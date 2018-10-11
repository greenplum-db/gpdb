create table t_update_hash_col_utilitymode(c int) distributed by (c);

1U: update t_update_hash_col_utilitymode set c = c + 1;

drop table t_update_hash_col_utilitymode;
