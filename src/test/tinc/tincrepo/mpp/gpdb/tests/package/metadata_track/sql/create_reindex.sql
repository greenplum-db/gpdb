create table mdt_test_reindex_tbl (col1 int,col2 int) distributed randomly;
create index clusterindex on mdt_test_reindex_tbl(col1);
REINDEX index "public".clusterindex;

drop table mdt_test_reindex_tbl;

