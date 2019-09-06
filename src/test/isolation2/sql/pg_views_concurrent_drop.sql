-- Helper function
CREATE or REPLACE FUNCTION lock_wait_until_ungranted()
RETURNS bool AS
$$
declare
retries int; /* in func */
begin /* in func */
  retries := 1200; /* in func */
  loop /* in func */
    if (select not granted from pg_locks where granted='f' and relation='concurrent_drop_view'::regclass) then /* in func */
      return true; /* in func */
    end if; /* in func */
    if retries <= 0 then /* in func */
      return false; /* in func */
    end if; /* in func */
    perform pg_sleep(0.1); /* in func */
    retries := retries - 1; /* in func */
  end loop; /* in func */
end; /* in func */
$$ language plpgsql;

1:drop view if exists concurrent_drop_view cascade;
1:create view concurrent_drop_view as select * from pg_class;
1:select viewname from pg_views where viewname = 'concurrent_drop_view';

-- One transaction drops the view, and before it commits, another
-- transaction selects its definition from pg_views. The view is still
-- visible to the second transaction, but deparsing the view's
-- definition will block until the first transaction ends. And if the
-- first transaction committed, the definition cannot be fetched.
-- It's returned as NULL in that case. (PostgreSQL throws an ERROR,
-- but getting a spurious ERROR when all you do is query pg_views is
-- not nice.)
1:begin;
1:drop view concurrent_drop_view;
2&:select viewname, definition from pg_views where viewname = 'concurrent_drop_view';
3:select lock_wait_until_ungranted();
1:commit;
2<:
