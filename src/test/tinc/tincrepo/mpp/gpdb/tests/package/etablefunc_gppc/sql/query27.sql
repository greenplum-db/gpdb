-- ETF call should fail for when input is not a TABLE value expression (TVE)
select * from transform( intable);

select * from transform( select id, value from intable);

select * from transform( t1_view);

select * from transform( select id, value from t1_view);
