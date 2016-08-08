--CLUSTER ON index_name & SET WITHOUT CLUSTER

          CREATE TABLE mdt_cluster_index_table (col1 int,col2 int) distributed randomly;

          create index mdt_clusterindex on mdt_cluster_index_table(col1);
          ALTER TABLE mdt_cluster_index_table CLUSTER on mdt_clusterindex;

          CREATE TABLE mdt_cluster_index_table1 (col1 int,col2 int) distributed randomly;

          create index mdt_clusterindex1 on mdt_cluster_index_table1(col1);
          ALTER TABLE mdt_cluster_index_table1 CLUSTER on mdt_clusterindex1;
          ALTER TABLE mdt_cluster_index_table1 SET WITHOUT CLUSTER;

drop table mdt_cluster_index_table;
drop table mdt_cluster_index_table1;
