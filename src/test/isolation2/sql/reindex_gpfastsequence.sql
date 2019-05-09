-- Test concurrent reindex gp_fastsequence and insert on an AO table

create table test_fastseqence ( a int, b char(20)) with (appendonly = true, orientation=column);
create index test_fastseqence_idx on test_fastseqence(b);
insert into test_fastseqence select i , 'aa'||i from generate_series(1,100) i;

select gp_inject_fault_infinite('reindex_relation', 'suspend', 2);

-- The reindex_relation fault should be hit
1&: reindex table gp_fastsequence;
2: select gp_wait_until_triggered_fault('reindex_relation', 1, 2);
-- The insert should for reindex in session 1
2&: insert into test_fastseqence select i , 'aa'||i from generate_series(1,100) i;

select gp_inject_fault('reindex_relation', 'reset', 2);

1<:
2<:

-- Validate that gp_fastsequence works as expected after reindex
SELECT 1 AS oid_same_on_all_segs from gp_dist_random('pg_class') WHERE
relname = 'gp_fastsequence_objid_objmod_index' GROUP BY oid
having count(*) = (SELECT count(*) FROM gp_segment_configuration WHERE
role='p' AND content > -1);

select last_sequence from gp_dist_random('gp_fastsequence') where
objid = (select segrelid from pg_appendonly where relid =
(select oid from pg_class where relname = 'test_fastseqence'));

insert into test_fastseqence select i , 'aa'||i from generate_series(1,100) i;

select last_sequence from gp_dist_random('gp_fastsequence') where
objid = (select segrelid from pg_appendonly where relid =
(select oid from pg_class where relname = 'test_fastseqence'));

-- Final validation should be performed by gpcheckcat, which runs at
-- the end of ICW in CI.
