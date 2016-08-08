select '1',ST_asewkt('POINT( 1 2 )'::GEOMETRY) as geom;
select '2',ST_asewkt('POINT( 1 2 3)'::GEOMETRY) as geom;

select '12',ST_asewkt('GEOMETRYCOLLECTION(POINT( 1 2 ))'::GEOMETRY);
select '13',ST_asewkt('GEOMETRYCOLLECTION(POINT( 1 2 3))'::GEOMETRY);

select '17',ST_asewkt('GEOMETRYCOLLECTION(POINT( 1 2 0),POINT( 1 2 3) )'::GEOMETRY);

