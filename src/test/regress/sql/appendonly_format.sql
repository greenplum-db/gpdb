-- Test dealing with Append-only tables created with previous version
-- of GPDB.
--
-- You cannot have such old-format segments in a cluster that's been
-- initialized with the current version, but with pg_upgrade you can. To
-- keep things simple, we don't want to involve pg_upgrade in these tests,
-- however. Instead, hack the catalogs directly to get the same situation
-- that you would have after pg_upgrade.


-- GPDB 4.3 -> GPDB 5.0 numerics conversion.
--
-- The on-disk representation of numerics changed between PostgreSQL 8.2
-- and 8.3. The weight and scale fields in the NumericData struct were
-- swapped. Construct an old-format Datum, by creating a dummy cast from
-- bytea to numeric that allows us to treat any binary blob as numeric,
-- and insert that into a table. Finally, hack the pg_aoseg catalog to
-- change the version number.
--
-- Note: Numeric columns need stricter alignment than byteas. Having
-- an int4 before the numeric should do the trick.

-- also test with a domain over numeric
CREATE DOMAIN numdomain AS numeric;

-- Construct the table, with the bytea
CREATE TABLE numeric_ao_upgrade (astext text, numcol numeric, domcol numdomain) with (appendonly = true);


begin;
create cast (bytea as numeric) without function;

INSERT INTO numeric_ao_upgrade VALUES
  ('1234.5678 and 9.876543210', E'\\000\\000\\004\\000\\322\\004\\056\\026'::bytea::numeric, E'\\000\\000\\011\\000\\011\\000\\075\\042\\341\\020'::bytea::numeric);

-- Drop the dangerous cast.
drop cast (bytea as numeric);
commit;


begin;
set local allow_system_table_mods = 'dml';

-- Hack the format version number in the pg_aoseg table. The name of the
-- table changes, so have to use dynamic SQL here.
CREATE FUNCTION pg_temp.tmpfunc() returns void as $$
begin
  execute 'update pg_aoseg.pg_aoseg_' || 'numeric_ao_upgrade'::regclass::oid ||
  	  ' set formatversion = 2';
end;
$$ language plpgsql;

select pg_temp.tmpfunc();

drop function pg_temp.tmpfunc();

commit;
