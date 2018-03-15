-- float4 check

CREATE TABLE float4tmp (a float4);

\copy float4tmp from 'data/float4.data'

SET enable_seqscan=on;

SELECT count(*) FROM float4tmp WHERE a <  -179.0;

SELECT count(*) FROM float4tmp WHERE a <= -179.0;

SELECT count(*) FROM float4tmp WHERE a  = -179.0;

SELECT count(*) FROM float4tmp WHERE a >= -179.0;

SELECT count(*) FROM float4tmp WHERE a >  -179.0;

CREATE INDEX float4idx ON float4tmp USING gist ( a );

SET enable_seqscan=off;

SELECT count(*) FROM float4tmp WHERE a <  -179.0::float4;

SELECT count(*) FROM float4tmp WHERE a <= -179.0::float4;

SELECT count(*) FROM float4tmp WHERE a  = -179.0::float4;

SELECT count(*) FROM float4tmp WHERE a >= -179.0::float4;

SELECT count(*) FROM float4tmp WHERE a >  -179.0::float4;
