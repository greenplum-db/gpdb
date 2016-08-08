1:drop view if exists mpp25160 cascade;
1:create view mpp25160 as select * from pg_class;
2:select viewname from pg_views where viewname = 'mpp25160';

-- This select should return no definition. The following drop
-- transaction should go through while this is asleep. Therefore,
-- make_viewdef() function should not be able open the view relation.
1&:select case when pg_sleep(5) is not null then definition end from pg_views where viewname = 'mpp25160';
2:drop view mpp25160 cascade;
1<:
1:select * from pg_views where viewname = 'mpp25160';
