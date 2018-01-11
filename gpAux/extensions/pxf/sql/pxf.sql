------------------------------------------------------------------
-- PXF read test
------------------------------------------------------------------

SELECT * from pxf_read_test order by a;
SELECT * from pxf_readcustom_test order by a;

------------------------------------------------------------------
-- PXF write test
------------------------------------------------------------------
\!rm -rf /tmp/pxf

INSERT INTO pxf_write_test SELECT * from origin;

\!ls -1 /tmp/pxf/ | wc -l | sed -e 's/^[[:space:]]*//'
\!cat /tmp/pxf/* | sort