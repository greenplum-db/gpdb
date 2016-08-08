create language plperl;
create language plpgsql;

-- Left Function
CREATE OR REPLACE FUNCTION "left"(text, integer)
  RETURNS character varying AS
$BODY$
my $string = shift; #get the passed string
my $length = shift; #get the passed length

$string =~ /^(.{$length})/;
return $1;
$BODY$
  LANGUAGE 'plperl' VOLATILE;

-- spr_rpt_sales_results_med function
CREATE OR REPLACE FUNCTION spr_rpt_sales_results_med()
  RETURNS integer AS
$BODY$
DECLARE
  l_row_id integer;
  l_row_max integer;
l_total_rows integer;
l_ama varchar;

begin

  l_row_id := 1;
  l_row_max := 5000;

  execute 'drop table if exists foo';
  execute 'create table foo as select i::bigint, ''ABCDEFGHI''::varchar as c from generate_series(1,5000000) i distributed randomly';

  while l_row_id <= l_row_max loop

  raise notice 'Loop - %', l_row_id;
  l_row_id := l_row_id + 1;

 execute 'select count(*) from foo where left( ''ABCDEFGH'', 3 )  = ''ABC'' ' into l_total_rows;

  end loop;

   return l_total_rows;
end;
$BODY$
  LANGUAGE 'plpgsql' VOLATILE;

select * from spr_rpt_sales_results_med();
