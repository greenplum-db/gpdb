create language plperl;


Drop function PLPERLinsert_wrong();
CREATE or REPLACE function  PLPERLinsert_wrong() RETURNS void as $$

	eval
        {
                spi_exec_query("INSERT INTO  errorhandlingtmptable VALUES ('fjdk');");
        };
        if ($@)
        {
                elog(NOTICE,"error occurs but ignore...\n");
                # hide exception
        }
	return;
$$ language PLPERL;

drop function subt_plperl_fn2 (int,int);
CREATE or replace function subt_plperl_fn2 (st int,en int) returns void as $$
DECLARE
i integer;
begin
  i=st;
  while i <= en LOOP
    perform PLPERLinsert_wrong();
    i = i + 1;
  END LOOP;
end;
$$ LANGUAGE 'plpgsql';


DROP table if exists subt_plperl_t2;
Create table subt_plperl_t2( i int);
select subt_plperl_fn1(1,200);
select count(*) from subt_plperl_t2;
