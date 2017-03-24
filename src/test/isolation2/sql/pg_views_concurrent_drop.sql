1:drop view if exists mpp25160 cascade;
1:create view mpp25160 as select * from pg_class;
1:select viewname from pg_views where viewname = 'mpp25160';

-- One transaction drops the view, and before it commits, another
-- transaction selects its definition from pg_views. The view is still
-- visible to the second transaction, but deparsing the view's
-- definition will block until the first transaction ends. And if the
-- first transaction committed, the definition cannot be fetched.
-- It's returned as NULL in that case. (PostgreSQL throws an ERROR,
-- but getting a spurious ERROR when all you do is query pg_views is
-- not nice.)
1:begin;
1:drop view mpp25160;
2&:select viewname, definition from pg_views where viewname = 'mpp25160';
1:commit;
2<:
