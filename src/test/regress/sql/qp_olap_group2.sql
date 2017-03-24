--
-- One purpose of these tests is to make sure that ORCA can handle all these
-- queries, and not fall back to the Postgres planner. To detect that,
-- turn optimizer_trace_fallback on, and watch for "falling back to planner"
-- messages.
--
set optimizer_trace_fallback='on';

-- Query 1
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1;
-- Query 2
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1;
-- Query 3
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1;
-- Query 4
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1;
-- Query 5
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1;
-- Query 6
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1;
-- Query 7
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1;
-- Query 8
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1;
-- Query 9
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1;
-- Query 10
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1;
-- Query 11
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1;
-- Query 12
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1;
-- Query 13
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1;
-- Query 14
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1;
-- Query 15
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1;
-- Query 16
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1;
-- Query 17
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1;
-- Query 18
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1;
-- Query 19
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1;
-- Query 20
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1;
-- Query 21
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1;
-- Query 22
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1;
-- Query 23
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1;
-- Query 24
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1;
-- Query 25
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY sale.pn, product.pname;
-- Query 26
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY sale.pn, product.pname;
-- Query 27
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 28
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY sale.pn, product.pname;
-- Query 29
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 30
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 31
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY sale.pn, product.pname;
-- Query 32
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY sale.pn, product.pname;
-- Query 33
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 34
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY sale.pn, product.pname;
-- Query 35
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 36
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 37
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY sale.pn, product.pname;
-- Query 38
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY sale.pn, product.pname;
-- Query 39
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 40
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY sale.pn, product.pname;
-- Query 41
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 42
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 43
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY sale.pn, product.pname;
-- Query 44
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY sale.pn, product.pname;
-- Query 45
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 46
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY sale.pn, product.pname;
-- Query 47
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 48
SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY sale.pn, product.pname;
-- Query 49
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 50
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 51
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 52
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 53
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 54
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 55
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 56
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 57
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 58
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 59
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 60
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 61
SELECT GROUPING(product.pname) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 62
SELECT GROUPING(product.pname) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 63
SELECT GROUPING(product.pname) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 64
SELECT GROUPING(product.pname) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 65
SELECT GROUPING(product.pname) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 66
SELECT GROUPING(product.pname) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 67
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 68
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 69
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 70
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 71
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 72
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 73
SELECT GROUPING(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 74
SELECT GROUPING(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 75
SELECT GROUPING(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 76
SELECT GROUPING(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 77
SELECT GROUPING(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 78
SELECT GROUPING(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 79
SELECT GROUPING(sale.pn) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 80
SELECT GROUPING(sale.pn) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 81
SELECT GROUPING(sale.pn) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 82
SELECT GROUPING(sale.pn) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 83
SELECT GROUPING(sale.pn) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 84
SELECT GROUPING(sale.pn) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 85
SELECT GROUPING(sale.pn) + 1 as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 86
SELECT GROUPING(sale.pn) + 1 as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 87
SELECT GROUPING(sale.pn) + 1 as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 88
SELECT GROUPING(sale.pn) + 1 as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 89
SELECT GROUPING(sale.pn) + 1 as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 90
SELECT GROUPING(sale.pn) + 1 as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 91
SELECT GROUPING(sale.pn) + 1 as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 92
SELECT GROUPING(sale.pn) + 1 as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 93
SELECT GROUPING(sale.pn) + 1 as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 94
SELECT GROUPING(sale.pn) + 1 as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 95
SELECT GROUPING(sale.pn) + 1 as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 96
SELECT GROUPING(sale.pn) + 1 as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 97
SELECT SUM(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 98
SELECT SUM(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 99
SELECT SUM(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 100
SELECT SUM(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 101
SELECT SUM(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 102
SELECT SUM(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 103
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 104
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 105
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 106
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 107
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 108
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 109
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1,g2;
-- Query 110
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1,g2;
-- Query 111
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1,g2;
-- Query 112
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1,g2;
-- Query 113
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1,g2;
-- Query 114
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1,g2;
-- Query 115
SELECT GROUPING(product.pname) as g1, sale.pn as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname,sale.pn ORDER BY g1,g2;
-- Query 116
SELECT GROUPING(sale.pn) as g1, sale.pn as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname,sale.pn ORDER BY g1,g2;
-- Query 117
SELECT GROUPING(sale.pn) + 1 as g1, sale.pn as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname,sale.pn ORDER BY g1,g2;
-- Query 118
SELECT SUM(sale.pn) as g1, sale.pn as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname,sale.pn ORDER BY g1,g2;
-- Query 119
SELECT GROUPING(product.pname) as g1, 'CONST' as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname,sale.pn ORDER BY g1,g2;
-- Query 120
SELECT GROUPING(sale.pn) as g1, 'CONST' as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname,sale.pn ORDER BY g1,g2;
-- Query 121
SELECT GROUPING(sale.pn) + 1 as g1, 'CONST' as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname,sale.pn ORDER BY g1,g2;
-- Query 122
SELECT SUM(sale.pn) as g1, 'CONST' as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname,sale.pn ORDER BY g1,g2;
-- Query 123
SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.cn,product.pname,sale.pn ORDER BY sale.cn;
-- Query 124
SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.cn,product.pname,sale.pn ORDER BY sale.cn;
-- Query 125
SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.cn,product.pname,sale.pn ORDER BY sale.cn;
-- Query 126
-- order 1
select 'a',* from (SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.cn,product.pname,sale.pn ORDER BY sale.cn) a;
-- Query 127
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 128
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 129
SELECT GROUPING(product.pname) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 130
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 131
SELECT GROUPING(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 132
SELECT GROUPING(sale.pn) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 133
SELECT GROUPING(sale.pn) + 1 as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 134
SELECT GROUPING(sale.pn) + 1 as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 135
SELECT SUM(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 136
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 137
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY product.pname, sale.pn ORDER BY g1,g2;
-- Query 138
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 139
SELECT GROUPING(product.pname) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 140
SELECT GROUPING(product.pname) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 141
SELECT GROUPING(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 142
SELECT GROUPING(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 143
SELECT GROUPING(sale.pn) as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 144
SELECT GROUPING(sale.pn) + 1 as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 145
SELECT GROUPING(sale.pn) + 1 as g1, SUM(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 146
SELECT SUM(sale.pn) as g1, GROUPING(product.pname) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 147
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 148
SELECT SUM(sale.pn) as g1, GROUPING(sale.pn) + 1 as g2 FROM product, sale WHERE product.pn=sale.pn GROUP BY sale.pn, product.pname ORDER BY g1,g2;
-- Query 149
SELECT sale.pn, GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY 1,2;
-- Query 150
SELECT sale.pn, GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY 1,2;
-- Query 151
SELECT sale.pn, GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY 1,2;
-- Query 152
SELECT sale.pn, GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY 1,2;
-- Query 153
SELECT sale.pn, GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY 1,2;
-- Query 154
SELECT sale.pn, GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY 1,2;
-- Query 155
SELECT sale.pn, GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY 1,2;
-- Query 156
SELECT sale.pn, GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY 1,2;
-- Query 157
SELECT sale.pn, GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY 1,2;
-- Query 158
SELECT sale.pn, GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY 1,2;
-- Query 159
SELECT sale.pn, GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY 1,2;
-- Query 160
SELECT sale.pn, GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY 1,2;
-- Query 161
SELECT sale.pn, GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY 1,2;
-- Query 162
SELECT sale.pn, GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY 1,2;
-- Query 163
SELECT sale.pn, GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY 1,2;
-- Query 164
SELECT sale.pn, GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY 1,2;
-- Query 165
SELECT sale.pn, GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY 1,2;
-- Query 166
SELECT sale.pn, GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY 1,2;
-- Query 167
SELECT sale.pn, SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY 1,2;
-- Query 168
SELECT sale.pn, SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY 1,2;
-- Query 169
SELECT sale.pn, SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY 1,2;
-- Query 170
SELECT sale.pn, SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY 1,2;
-- Query 171
SELECT sale.pn, SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY 1,2;
-- Query 172
SELECT sale.pn, SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY 1,2;
-- Query 173
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1) UNION (SELECT sale.pn FROM sale)) as a;
-- Query 174
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1) UNION (SELECT sale.pn FROM sale) )a;
-- Query 175
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale) )a;
-- Query 176
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1) UNION (SELECT sale.pn FROM sale) )a;
-- Query 177
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 178
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 179
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 180
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 181
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 182
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 183
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 184
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 185
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 186
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 187
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 188
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 189
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 190
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 191
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 192
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 193
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 194
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 195
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 196
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1) UNION (SELECT sale.pn FROM sale))a;
-- Query 197
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1) UNION (SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1))a;
-- Query 198
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1) UNION (SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1))a;
-- Query 199
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1))a;
-- Query 200
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1) UNION (SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1))a;
-- Query 201
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1))a;
-- Query 202
-- order 1
select 'a', * from ((SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(product.pname) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1))a;
-- Query 203
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1))a;
-- Query 204
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1))a;
-- Query 205
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1))a;
-- Query 206
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1))a;
-- Query 207
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1))a;
-- Query 208
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1))a;
-- Query 209
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1))a;
-- Query 210
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1))a;
-- Query 211
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1))a;
-- Query 212
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1))a;
-- Query 213
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1))a;
-- Query 214
-- order 1
select 'a', * from ((SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1) UNION (SELECT GROUPING(sale.pn) + 1 as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1))a;
-- Query 215
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1) UNION (SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname) ORDER BY g1))a;
-- Query 216
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1) UNION (SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS (sale.pn, product.pname, sale.pn) ORDER BY g1))a;
-- Query 217
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1) UNION (SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY GROUPING SETS ((sale.pn) ,(product.pname, sale.pn)) ORDER BY g1))a;
-- Query 218
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1) UNION (SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname)) ORDER BY g1))a;
-- Query 219
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1) UNION (SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn,product.pname,sale.pn)) ORDER BY g1)) a;
-- Query 220
-- order 1
select 'a', * from ((SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1) UNION (SELECT SUM(sale.pn) as g1 FROM product, sale WHERE product.pn=sale.pn GROUP BY ROLLUP((sale.pn),(product.pname),(sale.pn)) ORDER BY g1))a;
