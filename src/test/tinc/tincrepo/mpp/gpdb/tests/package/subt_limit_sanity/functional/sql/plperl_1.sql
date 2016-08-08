create language plperl;


Drop function PLPERLinsert_correct();
CREATE or REPLACE function  PLPERLinsert_correct () RETURNS void as $$
        spi_exec_query('INSERT INTO  subt_plperl_t1 VALUES (1);');
        spi_exec_query('INSERT INTO  subt_plperl_t1 VALUES (2);');
        spi_exec_query('INSERT INTO  subt_plperl_t1 VALUES (4);');
	return;
$$ language PLPERL;

drop function subt_plperl_fn1 (int,int);
CREATE or replace function subt_plperl_fn1 (st int,en int) returns void as $$
DECLARE
i integer;
begin
  i=st;
  while i <= en LOOP
    perform PLPERLinsert_correct();
    i = i + 1;
  END LOOP;
end;
$$ LANGUAGE 'plpgsql';


DROP table if exists subt_plperl_t1;
Create table subt_plperl_t1 ( i int);
select subt_plperl_fn1(1,200);
select count(*) from subt_plperl_t1;
