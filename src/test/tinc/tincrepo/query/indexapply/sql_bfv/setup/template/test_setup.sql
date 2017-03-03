drop table x;
drop table y;
create table x ( i %datatype%, j %datatype%, k %datatype%, m %datatype%) distributed by (%xDIST%) %xPART%;
create table y ( i %datatype%, j %datatype%, k %datatype%, m %datatype%) distributed by (%yDIST%) %yPART%;
COPY x (i, j, k, m) FROM '%MYD%/data/x_%datatype%.txt' with DELIMITER AS ',';
COPY y (i, j, k, m) FROM '%MYD%/data/y_%datatype%.txt' with DELIMITER AS ',';
--create INDEX x_idx on x using %idxtype%(%xIDX%);
create INDEX x_idx on x (%xIDX%);
create INDEX y_idx on y (%yIDX%);

create function plusone(x int) RETURNS int AS $$
BEGIN
    RETURN x + 5;
END;
$$ language plpgsql;


create function plusone(x varchar) RETURNS varchar AS $$
BEGIN
    RETURN x || 'a';
END;
$$ language plpgsql;


create function plusone(x text) RETURNS text AS $$
BEGIN
    RETURN x || 'x';
END;
$$ language plpgsql;
