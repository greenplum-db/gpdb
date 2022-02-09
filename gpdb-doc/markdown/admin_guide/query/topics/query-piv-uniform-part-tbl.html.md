---
title: About Uniform Multi-level Partitioned Tables 
---

GPORCA supports queries on a multi-level partitioned \(MLP\) table if the MLP table is a uniform partitioned table. A multi-level partitioned table is a partitioned table that was created with the `SUBPARTITION` clause. A uniform partitioned table must meet these requirements.

-   The partitioned table structure is uniform. Each partition node at the same level must have the same hierarchical structure.
-   The partition key constraints must be consistent and uniform. At each subpartition level, the sets of constraints on the child tables created for each branch must match.

You can display information about partitioned tables in several ways, including displaying information from these sources:

-   The `pg_partitions` system view contains information on the structure of a partitioned table.
-   The `pg_constraint` system catalog table contains information on table constraints.
-   The psql meta command \\d+ tablename displays the table constraints for child leaf tables of a partitioned table.

**Parent topic:**[About GPORCA](../../query/topics/query-piv-optimizer.html)

## <a id="topic_zn1_m3m_ss"></a>Example 

This `CREATE TABLE` command creates a uniform partitioned table.

```
CREATE TABLE mlp (id  int,  year int,  month int,  day int,
   region  text)
   DISTRIBUTED  BY (id)
    PARTITION BY RANGE ( year)
      SUBPARTITION  BY LIST (region)
        SUBPARTITION TEMPLATE (
          SUBPARTITION usa  VALUES ( 'usa'),
          SUBPARTITION europe  VALUES ( 'europe'),
          SUBPARTITION asia  VALUES ( 'asia'))
   (  START ( 2006)  END ( 2016) EVERY ( 5));
```

These are child tables and the partition hierarchy that are created for the table `mlp`. This hierarchy consists of one subpartition level that contains two branches.

```
mlp_1_prt_11
   mlp_1_prt_11_2_prt_usa
   mlp_1_prt_11_2_prt_europe
   mlp_1_prt_11_2_prt_asia

mlp_1_prt_21
   mlp_1_prt_21_2_prt_usa
   mlp_1_prt_21_2_prt_europe
   mlp_1_prt_21_2_prt_asia
```

The hierarchy of the table is uniform, each partition contains a set of three child tables \(subpartitions\). The constraints for the region subpartitions are uniform, the set of constraints on the child tables for the branch table `mlp_1_prt_11` are the same as the constraints on the child tables for the branch table `mlp_1_prt_21`.

As a quick check, this query displays the constraints for the partitions.

```
WITH tbl AS (SELECT oid, partitionlevel AS level, 
             partitiontablename AS part 
         FROM pg_partitions, pg_class 
         WHERE tablename = 'mlp' AND partitiontablename=relname 
            AND partitionlevel=1 ) 
  SELECT tbl.part, consrc 
    FROM tbl, pg_constraint 
    WHERE tbl.oid = conrelid ORDER BY consrc;
```

**Note:** You will need modify the query for more complex partitioned tables. For example, the query does not account for table names in different schemas.

The `consrc` column displays constraints on the subpartitions. The set of region constraints for the subpartitions in `mlp_1_prt_1` match the constraints for the subpartitions in `mlp_1_prt_2`. The constraints for year are inherited from the parent branch tables.

```
           part           |               consrc
--------------------------+------------------------------------
 mlp_1_prt_2_2_prt_asia   | (region = 'asia'::text)
 mlp_1_prt_1_2_prt_asia   | (region = 'asia'::text)
 mlp_1_prt_2_2_prt_europe | (region = 'europe'::text)
 mlp_1_prt_1_2_prt_europe | (region = 'europe'::text)
 mlp_1_prt_1_2_prt_usa    | (region = 'usa'::text)
 mlp_1_prt_2_2_prt_usa    | (region = 'usa'::text)
 mlp_1_prt_1_2_prt_asia   | ((year >= 2006) AND (year < 2011))
 mlp_1_prt_1_2_prt_usa    | ((year >= 2006) AND (year < 2011))
 mlp_1_prt_1_2_prt_europe | ((year >= 2006) AND (year < 2011))
 mlp_1_prt_2_2_prt_usa    | ((year >= 2011) AND (year < 2016))
 mlp_1_prt_2_2_prt_asia   | ((year >= 2011) AND (year < 2016))
 mlp_1_prt_2_2_prt_europe | ((year >= 2011) AND (year < 2016))
(12 rows)
```

If you add a default partition to the example partitioned table with this command:

```
ALTER TABLE mlp ADD DEFAULT PARTITION def
```

The partitioned table remains a uniform partitioned table. The branch created for default partition contains three child tables and the set of constraints on the child tables match the existing sets of child table constraints.

In the above example, if you drop the subpartition `mlp_1_prt_21_2_prt_asia` and add another subpartition for the region `canada`, the constraints are no longer uniform.

```
ALTER TABLE mlp ALTER PARTITION FOR (RANK(2))
  DROP PARTITION asia ;

ALTER TABLE mlp ALTER PARTITION FOR (RANK(2))
  ADD PARTITION canada VALUES ('canada');
```

Also, if you add a partition `canada` under `mlp_1_prt_21`, the partitioning hierarchy is not uniform.

However, if you add the subpartition `canada` to both `mlp_1_prt_21` and `mlp_1_prt_11` the of the original partitioned table, it remains a uniform partitioned table.

**Note:** Only the constraints on the sets of partitions at a partition level must be the same. The names of the partitions can be different.

