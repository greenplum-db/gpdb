create or replace function test_spi_tup(text) returns setof record as '
  pg.spi.exec(arg1)
' language 'plr';

select * from test_spi_tup('select oid, NULL::text as nullcol,
  typname from pg_type where typname = ''oid'' or typname = ''text''')
  as t(typeid oid, nullcol text, typename name);
