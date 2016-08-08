\echo -- Your distribution should be compiled with --with-perl.
drop language plperl cascade;
create language plperl;
drop table if exists test cascade;
create table test (i integer, v varchar);

CREATE OR REPLACE FUNCTION valid_i() RETURNS trigger AS $$
    if (($_TD->{new}{i} >= 100) || ($_TD->{new}{i} <= 0)) {
        return "SKIP";    # skip INSERT/UPDATE command
    } elsif ($_TD->{new}{v} ne "immortal") {
        $_TD->{new}{v} .= "(modified by trigger)";
        return "MODIFY";  # modify row and execute INSERT/UPDATE command
    } else {
        return;           # execute INSERT/UPDATE command
    }
$$ LANGUAGE plperl;

CREATE TRIGGER test_valid_id_trig
    BEFORE INSERT OR UPDATE ON test
    FOR EACH ROW EXECUTE PROCEDURE valid_i();

insert into test values (1,'1'), (2,'2'), (3,'3');

update test set v = '10' where v='1';

drop trigger test_valid_id_trig on test;
drop function valid_i();
drop language plperl;
