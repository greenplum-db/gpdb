CREATE TABLE T(url TEXT) distributed BY (url);
INSERT INTO T (SELECT 'www.widgets' || num::TEXT || '.com' FROM generate_series(1, 100) AS num);
INSERT INTO T (SELECT 'www.widgets' || num::TEXT || '.com' FROM generate_series(1, 100) AS num);
SELECT uuid_generate_v3(uuid_ns_dns(), t.url) FROM T;
SELECT uuid_generate_v5(uuid_ns_dns(), t.url) FROM T;

WITH T(ID) AS
	(SELECT uuid_generate_v1() FROM generate_series(1, 10000))
SELECT count(DISTINCT(ID)) FROM T;

WITH T(ID) AS
	(SELECT uuid_generate_v1mc() FROM generate_series(1, 10000))
SELECT count(DISTINCT(ID)) FROM T;

WITH T(ID) AS
	(SELECT uuid_generate_v4() FROM generate_series(1, 10000))
SELECT count(DISTINCT(ID)) FROM T;
