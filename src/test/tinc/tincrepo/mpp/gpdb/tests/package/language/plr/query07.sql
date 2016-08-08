drop language if exists 'plr' cascade;
create language 'plr';

drop function if exists r_null(param integer[]);
create or replace function r_null(param integer[]) returns text as '
	return("done")
' language 'plr';

select r_null(ARRAY[1, 2, 3]);

select r_null(ARRAY[1, 2, null]);