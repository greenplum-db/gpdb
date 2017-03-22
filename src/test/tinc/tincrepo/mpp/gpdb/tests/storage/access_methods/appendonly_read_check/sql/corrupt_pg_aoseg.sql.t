set allow_system_table_mods='DML';

--pgaoseg has more entries than gp_relation_node
begin;
    insert into pg_aoseg.pg_aoseg_<oid> values(5, 6, 7, 8, 9, 10, 11, 12);
    select * from ao1;
abort;

--pgaoseg has less entries than gp_relation_node
begin;
    delete from pg_aoseg.pg_aoseg_<oid>;
    select * from ao1;
abort;

--pgaoseg has same number of entries but mismatch with gp_relation_node
begin;
    update pg_aoseg.pg_aoseg_<oid> set segno = 3 where segno = 1;
    select * from ao1;
abort;

select * from ao1; --Make sure we can still select from ao1
