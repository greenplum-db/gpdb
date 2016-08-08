select * from gp_configuration;
create table p (a int) partition by list (a) (partition a values(1), partition b values (2));
create role newrole;
\dt p*
alter table p owner to newrole;
\dt p*
PGOPTIONS='-c gp_session_role=utility' psql -p 48507 gptest -c '\dt p*'
PGOPTIONS='-c gp_session_role=utility' psql -p 48508 gptest -c '\dt p*'
PGOPTIONS='-c gp_session_role=utility' psql -p 48509 gptest -c '\dt p*'
