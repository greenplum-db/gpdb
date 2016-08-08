select '23',ST_asewkt('MULTIPOINT( 1 2)'::GEOMETRY) as geom;
select '24',ST_asewkt('MULTIPOINT( 1 2 3)'::GEOMETRY) as geom;
select '25',ST_asewkt('MULTIPOINT( 1 2, 3 4, 5 6)'::GEOMETRY) as geom;
select '26',ST_asewkt('MULTIPOINT( 1 2 3, 5 6 7, 8 9 10, 11 12 13)'::GEOMETRY) as geom;
select '27',ST_asewkt('MULTIPOINT( 1 2 0, 1 2 3, 4 5 0, 6 7 8)'::GEOMETRY) as geom;
select '28',ST_asewkt('MULTIPOINT( 1 2 3,4 5 0)'::GEOMETRY) as geom;

select '36',ST_asewkt('GEOMETRYCOLLECTION(MULTIPOINT( 1 2))'::GEOMETRY);
select '37',ST_asewkt('GEOMETRYCOLLECTION(MULTIPOINT( 1 2 3))'::GEOMETRY);
select '38',ST_asewkt('GEOMETRYCOLLECTION(MULTIPOINT( 1 2 3, 5 6 7, 8 9 10, 11 12 13))'::GEOMETRY);

