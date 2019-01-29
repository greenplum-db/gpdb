select count(1) from lineitem as a, lineitem as b where a.l_orderkey = b.l_orderkey;
