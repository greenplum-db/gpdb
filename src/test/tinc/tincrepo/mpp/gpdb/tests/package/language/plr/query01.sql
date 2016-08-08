create or replace function sd(_float8) returns float as '' language 'plr';
select round(sd('{1.23,1.31,1.42,1.27}'::_float8)::numeric,8);
