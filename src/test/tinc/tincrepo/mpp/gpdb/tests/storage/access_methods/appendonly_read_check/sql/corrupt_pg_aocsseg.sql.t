set allow_system_table_mods='DML';

--pgaocsseg has more entries than gp_relation_node
begin;
    insert into pg_aoseg.pg_aocsseg_<oid> values(5, 6, 7, '0'::bytea, 1, 1, 1);
    select * from co1;
abort;

--pgaocsseg has less entries than gp_relation_node
begin;
    delete from pg_aoseg.pg_aocsseg_<oid>;
    select * from co1;
abort;

--pgaocsseg has same number of entries but mistmatch with gp_relation_node
begin;
    update pg_aoseg.pg_aocsseg_<oid> set segno = 3 where segno = 1;
    select * from co1;
abort;

select * from co1; --Make sure we can still select from co1
