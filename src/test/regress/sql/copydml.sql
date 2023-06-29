--
-- Test cases for COPY (INSERT/UPDATE/DELETE) TO
--
create sequence copydml_test_id_seq cache 1;
create table copydml_test (id int default nextval('copydml_test_id_seq'), t text);
insert into copydml_test (t) values ('a');
insert into copydml_test (t) values ('b');
insert into copydml_test (t) values ('c');
insert into copydml_test (t) values ('d');
insert into copydml_test (t) values ('e');

--
-- Test COPY (insert/update/delete ...)
--
copy (insert into copydml_test (t) values ('f') returning id) to stdout;
copy (update copydml_test set t = 'g' where t = 'f' returning id) to stdout;
copy (delete from copydml_test where t = 'g' returning id) to stdout;

--
-- Test \copy (insert/update/delete ...)
--
\copy (insert into copydml_test (t) values ('f') returning id) to stdout;
\copy (update copydml_test set t = 'g' where t = 'f' returning id) to stdout;
\copy (delete from copydml_test where t = 'g' returning id) to stdout;

-- Error cases
copy (insert into copydml_test default values) to stdout;
copy (update copydml_test set t = 'g') to stdout;
copy (delete from copydml_test) to stdout;

create rule qqq as on insert to copydml_test do instead nothing;
copy (insert into copydml_test default values) to stdout;
drop rule qqq on copydml_test;
create rule qqq as on insert to copydml_test do also delete from copydml_test;
copy (insert into copydml_test default values) to stdout;
drop rule qqq on copydml_test;
create rule qqq as on insert to copydml_test do instead (delete from copydml_test; delete from copydml_test);
copy (insert into copydml_test default values) to stdout;
drop rule qqq on copydml_test;
create rule qqq as on insert to copydml_test where new.t <> 'f' do instead delete from copydml_test;
copy (insert into copydml_test default values) to stdout;
drop rule qqq on copydml_test;

create rule qqq as on update to copydml_test do instead nothing;
copy (update copydml_test set t = 'f') to stdout;
drop rule qqq on copydml_test;
create rule qqq as on update to copydml_test do also delete from copydml_test;
copy (update copydml_test set t = 'f') to stdout;
drop rule qqq on copydml_test;
create rule qqq as on update to copydml_test do instead (delete from copydml_test; delete from copydml_test);
copy (update copydml_test set t = 'f') to stdout;
drop rule qqq on copydml_test;
create rule qqq as on update to copydml_test where new.t <> 'f' do instead delete from copydml_test;
copy (update copydml_test set t = 'f') to stdout;
drop rule qqq on copydml_test;

create rule qqq as on delete to copydml_test do instead nothing;
copy (delete from copydml_test) to stdout;
drop rule qqq on copydml_test;
create rule qqq as on delete to copydml_test do also insert into copydml_test default values;
copy (delete from copydml_test) to stdout;
drop rule qqq on copydml_test;
create rule qqq as on delete to copydml_test do instead (insert into copydml_test default values; insert into copydml_test default values);
copy (delete from copydml_test) to stdout;
drop rule qqq on copydml_test;
create rule qqq as on delete to copydml_test where old.t <> 'f' do instead insert into copydml_test default values;
copy (delete from copydml_test) to stdout;
drop rule qqq on copydml_test;

-- triggers
create function qqq_trig() returns trigger as $$
begin

-- In GPDB, the tuple is sent from the QE to QD via the interconnect, while
-- the NOTICE is sent via the main QE-QD libpq connection. It's not
-- deterministic, which one reaches the QD first. Add a little sleep here
-- to make it more consistent, for the sake of having stable expected output.
--
-- Even with the sleep, there's no guarantee. One known inconsistency at the
-- moment is that the UDP interconnect buffers tuples differently. So despite
-- the sleep, we need an alternative expected output file for the case that
-- all the NOTICEs arrive before the tuples, but at least we don't need an
-- output file for every combination of orderings of the NOTICEs and the
-- tuples, in the three COPY (...) TO STDOUT commands below. It may seem odd
-- that the BEFORE NOTICE arrives only after the tuple, despite this sleep,
-- but it's not well defined when the QD drains the NOTICes from the libpq
-- connection relative to the interconnect data.
perform pg_sleep(0.2);

if tg_op in ('INSERT', 'UPDATE') then
    raise notice '% % %', tg_when, tg_op, new.id;
    return new;
else
    raise notice '% % %', tg_when, tg_op, old.id;
    return old;
end if;
end
$$ language plpgsql;
create trigger qqqbef before insert or update or delete on copydml_test
    for each row execute procedure qqq_trig();
create trigger qqqaf after insert or update or delete on copydml_test
    for each row execute procedure qqq_trig();

copy (insert into copydml_test (t) values ('f') returning id) to stdout;
copy (update copydml_test set t = 'g' where t = 'f' returning id) to stdout;
copy (delete from copydml_test where t = 'g' returning id) to stdout;

drop table copydml_test;
drop function qqq_trig();
drop sequence copydml_test_id_seq;
